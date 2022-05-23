#include "datashard_kqp_compute.h"
#include "range_ops.h"

#include <ydb/core/kqp/runtime/kqp_transport.h>
#include <ydb/core/kqp/runtime/kqp_read_table.h>
#include <ydb/core/kqp/runtime/kqp_scan_data.h>
#include <ydb/core/tx/datashard/datashard_impl.h>

#include <ydb/library/yql/minikql/mkql_node.h>

namespace NKikimr {
namespace NMiniKQL {

using namespace NTable;
using namespace NUdf;

TSmallVec<TTag> ExtractTags(const TSmallVec<TKqpComputeContextBase::TColumn>& columns) {
    TSmallVec<TTag> tags;
    for (const auto& column : columns) {
        tags.push_back(column.Tag);
    }
    return tags;
}

typedef IComputationNode* (*TCallableDatashardBuilderFunc)(TCallable& callable,
    const TComputationNodeFactoryContext& ctx, TKqpDatashardComputeContext& computeCtx);

struct TKqpDatashardComputationMap {
    TKqpDatashardComputationMap() {
        Map["KqpWideReadTable"] = &WrapKqpWideReadTable;
        Map["KqpWideReadTableRanges"] = &WrapKqpWideReadTableRanges;
        Map["KqpLookupTable"] = &WrapKqpLookupTable;
        Map["KqpUpsertRows"] = &WrapKqpUpsertRows;
        Map["KqpDeleteRows"] = &WrapKqpDeleteRows;
        Map["KqpEffects"] = &WrapKqpEffects;
    }

    THashMap<TString, TCallableDatashardBuilderFunc> Map;
};

TComputationNodeFactory GetKqpDatashardComputeFactory(TKqpDatashardComputeContext* computeCtx) {
    MKQL_ENSURE_S(computeCtx);
    MKQL_ENSURE_S(computeCtx->Database);

    auto computeFactory = GetKqpBaseComputeFactory(computeCtx);

    return [computeFactory, computeCtx]
        (TCallable& callable, const TComputationNodeFactoryContext& ctx) -> IComputationNode* {
            if (auto compute = computeFactory(callable, ctx)) {
                return compute;
            }

            const auto& datashardMap = Singleton<TKqpDatashardComputationMap>()->Map;
            auto it = datashardMap.find(callable.GetType()->GetName());
            if (it != datashardMap.end()) {
                return it->second(callable, ctx, *computeCtx);
            }

            return nullptr;
        };
};

typedef IComputationNode* (*TCallableScanBuilderFunc)(TCallable& callable,
    const TComputationNodeFactoryContext& ctx, TKqpScanComputeContext& computeCtx);

struct TKqpScanComputationMap {
    TKqpScanComputationMap() {
        Map["KqpWideReadTable"] = &WrapKqpScanWideReadTable;
        Map["KqpWideReadTableRanges"] = &WrapKqpScanWideReadTableRanges;
    }

    THashMap<TString, TCallableScanBuilderFunc> Map;
};

TComputationNodeFactory GetKqpScanComputeFactory(TKqpScanComputeContext* computeCtx) {
    MKQL_ENSURE_S(computeCtx);

    auto computeFactory = GetKqpBaseComputeFactory(computeCtx);

    return [computeFactory, computeCtx]
        (TCallable& callable, const TComputationNodeFactoryContext& ctx) -> IComputationNode* {
            if (auto compute = computeFactory(callable, ctx)) {
                return compute;
            }

            const auto& datashardMap = Singleton<TKqpScanComputationMap>()->Map;
            auto it = datashardMap.find(callable.GetType()->GetName());
            if (it != datashardMap.end()) {
                return it->second(callable, ctx, *computeCtx);
            }

            return nullptr;
        };
}

ui64 TKqpDatashardComputeContext::GetLocalTableId(const TTableId &tableId) const {
    MKQL_ENSURE_S(Shard);
    return Shard->GetLocalTableId(tableId);
}

const NDataShard::TUserTable::TUserColumn& TKqpDatashardComputeContext::GetKeyColumnInfo(
    const NDataShard::TUserTable& table, ui32 keyIndex) const
{
    MKQL_ENSURE_S(keyIndex <= table.KeyColumnTypes.size());
    const auto& col = table.Columns.at(table.KeyColumnIds[keyIndex]);
    MKQL_ENSURE_S(col.IsKey);

    return col;
}

THashMap<TString, NScheme::TTypeId> TKqpDatashardComputeContext::GetKeyColumnsMap(const TTableId &tableId) const {
    MKQL_ENSURE_S(Shard);
    const NDataShard::TUserTable::TCPtr* tablePtr = Shard->GetUserTables().FindPtr(tableId.PathId.LocalPathId);
    MKQL_ENSURE_S(tablePtr);
    const NDataShard::TUserTable::TCPtr table = *tablePtr;
    MKQL_ENSURE_S(table);

    THashMap<TString, NScheme::TTypeId> columnsMap;
    for (size_t i = 0 ; i < table->KeyColumnTypes.size(); i++) {
        auto col = table->Columns.at(table->KeyColumnIds[i]);
        MKQL_ENSURE_S(col.IsKey);
        columnsMap[col.Name] = col.Type;

    }

    return columnsMap;
}

TString TKqpDatashardComputeContext::GetTablePath(const TTableId &tableId) const {
    MKQL_ENSURE_S(Shard);

    auto table = Shard->GetUserTables().FindPtr(tableId.PathId.LocalPathId);
    if (!table) {
        return TStringBuilder() << tableId;
    }

    return (*table)->Path;
}

const NDataShard::TUserTable* TKqpDatashardComputeContext::GetTable(const TTableId& tableId) const {
    MKQL_ENSURE_S(Shard);
    auto ptr = Shard->GetUserTables().FindPtr(tableId.PathId.LocalPathId);
    MKQL_ENSURE_S(ptr);
    return ptr->Get();
}

void TKqpDatashardComputeContext::TouchTableRange(const TTableId& tableId, const TTableRange& range) const {
    Shard->SysLocksTable().SetLock(tableId, range, LockTxId);
    Shard->SetTableAccessTime(tableId, Now);
}

void TKqpDatashardComputeContext::TouchTablePoint(const TTableId& tableId, const TArrayRef<const TCell>& key) const {
    Shard->SysLocksTable().SetLock(tableId, key, LockTxId);
    Shard->SetTableAccessTime(tableId, Now);
}

void TKqpDatashardComputeContext::BreakSetLocks() const {
    Shard->SysLocksTable().BreakSetLocks(LockTxId);
}

void TKqpDatashardComputeContext::SetLockTxId(ui64 lockTxId) {
    LockTxId = lockTxId;
}

void TKqpDatashardComputeContext::SetReadVersion(TRowVersion readVersion) {
    ReadVersion = readVersion;
}

TRowVersion TKqpDatashardComputeContext::GetReadVersion() const {
    Y_VERIFY(!ReadVersion.IsMin(), "Cannot perform reads without ReadVersion set");

    return ReadVersion;
}

void TKqpDatashardComputeContext::SetTaskOutputChannel(ui64 taskId, ui64 channelId, TActorId actorId) {
    OutputChannels.emplace(std::make_pair(taskId, channelId), actorId);
}

TActorId TKqpDatashardComputeContext::GetTaskOutputChannel(ui64 taskId, ui64 channelId) const {
    auto it = OutputChannels.find(std::make_pair(taskId, channelId));
    if (it != OutputChannels.end()) {
        return it->second;
    }
    return TActorId();
}

void TKqpDatashardComputeContext::Clear() {
    Database = nullptr;
    LockTxId = 0;
}

bool TKqpDatashardComputeContext::PinPages(const TVector<IEngineFlat::TValidatedKey>& keys, ui64 pageFaultCount) {
    ui64 limitMultiplier = 1;
    if (pageFaultCount >= 2) {
        if (pageFaultCount <= 63) {
            limitMultiplier <<= pageFaultCount - 1;
        } else {
            limitMultiplier = Max<ui64>();
        }
    }

    auto adjustLimit = [limitMultiplier](ui64 limit) -> ui64 {
        if (limit >= Max<ui64>() / limitMultiplier) {
            return Max<ui64>();
        } else {
            return limit * limitMultiplier;
        }
    };

    bool ret = true;
    auto& scheme = Database->GetScheme();

    for (const auto& vKey : keys) {
        const TKeyDesc& key = *vKey.Key;

        if (TSysTables::IsSystemTable(key.TableId)) {
            continue;
        }

        TSet<TKeyDesc::EColumnOperation> columnOpFilter;
        switch (key.RowOperation) {
            case TKeyDesc::ERowOperation::Read:
                columnOpFilter.insert(TKeyDesc::EColumnOperation::Read);
                break;
            case TKeyDesc::ERowOperation::Update:
            case TKeyDesc::ERowOperation::Erase: {
                const auto collector = EngineHost.GetChangeCollector(key.TableId);
                if (collector && collector->NeedToReadKeys()) {
                    columnOpFilter.insert(TKeyDesc::EColumnOperation::Set);
                    columnOpFilter.insert(TKeyDesc::EColumnOperation::InplaceUpdate);
                }
                break;
            }
            default:
                break;
        }

        if (columnOpFilter.empty()) {
            continue;
        }

        ui64 localTid = GetLocalTableId(key.TableId);
        Y_VERIFY(localTid, "table not exist");

        auto* tableInfo = scheme.GetTableInfo(localTid);
        TSmallVec<TRawTypeValue> from;
        TSmallVec<TRawTypeValue> to;
        ConvertTableKeys(scheme, tableInfo, key.Range.From, from, nullptr);
        if (!key.Range.Point) {
            ConvertTableKeys(scheme, tableInfo, key.Range.To, to, nullptr);
        }

        TSmallVec<NTable::TTag> columnTags;
        for (const auto& column : key.Columns) {
            if (columnOpFilter.contains(column.Operation)) {
                columnTags.push_back(column.Column);
            }
        }

        bool ready = Database->Precharge(localTid,
                                         from,
                                         key.Range.Point ? from : to,
                                         columnTags,
                                         EngineHost.GetSettings().DisableByKeyFilter ? (ui64)NTable::NoByKey : 0,
                                         adjustLimit(key.RangeLimits.ItemsLimit),
                                         adjustLimit(key.RangeLimits.BytesLimit),
                                         key.Reverse ? NTable::EDirection::Reverse : NTable::EDirection::Forward,
                                         GetReadVersion());

        LOG_TRACE_S(*TlsActivationContext, NKikimrServices::TX_DATASHARD, "Run precharge on table " << tableInfo->Name
            << ", columns: [" << JoinSeq(", ", columnTags) << "]"
            << ", range: " << DebugPrintRange(key.KeyColumnTypes, key.Range, *AppData()->TypeRegistry)
            << ", itemsLimit: " << key.RangeLimits.ItemsLimit
            << ", bytesLimit: " << key.RangeLimits.BytesLimit
            << ", reverse: " << key.Reverse
            << ", result: " << ready);

        ret &= ready;
    }

    return ret;
}

std::pair<IEngineFlat::EResult, TString> TKqpDatashardComputeContext::ValidateKeys(
    const IEngineFlat::TValidationInfo& validationInfo)
{
    std::pair<ui64, ui64> maxSnapshotTime = {0,0}; // unused for now
    for (auto& validKey : validationInfo.Keys) {
        TKeyDesc * key = validKey.Key.get();

        bool valid = EngineHost.IsValidKey(*key, maxSnapshotTime);

        if (valid) {
            auto curSchemaVersion = EngineHost.GetTableSchemaVersion(key->TableId);
            if (key->TableId.SchemaVersion && curSchemaVersion && curSchemaVersion != key->TableId.SchemaVersion) {
                auto error = TStringBuilder()
                    << "Schema version missmatch for table id: " << key->TableId
                    << " mkql compiled on: " << key->TableId.SchemaVersion
                    << " current version: " << curSchemaVersion;
                return {IEngineFlat::EResult::SchemeChanged, std::move(error)};
            }
        } else {
            switch (key->Status) {
                case TKeyDesc::EStatus::SnapshotNotExist:
                    return {IEngineFlat::EResult::SnapshotNotExist, ""};
                case TKeyDesc::EStatus::SnapshotNotReady:
                    key->Status = TKeyDesc::EStatus::Ok;
                    return {IEngineFlat::EResult::SnapshotNotReady, ""};
                default:
                    auto error = TStringBuilder()
                        << "Validate (" << __LINE__ << "): Key validation status: " << (ui32)key->Status;
                    return {IEngineFlat::EResult::KeyError, std::move(error)};
            }
        }
    }

    return {IEngineFlat::EResult::Ok, ""};
}

static void BuildRowImpl(const TDbTupleRef& dbTuple, const THolderFactory& holderFactory,
    const TSmallVec<TTag>& systemColumnTags, ui64 shardId, NUdf::TUnboxedValue& result, size_t& rowSize)
{
    size_t columnsCount = dbTuple.ColumnCount + systemColumnTags.size();

    TUnboxedValue* rowItems = nullptr;
    result = holderFactory.CreateDirectArrayHolder(columnsCount, rowItems);

    rowSize = 0;
    for (ui32 i = 0; i < dbTuple.ColumnCount; ++i) {
        const auto& cell = dbTuple.Cells()[i];
        rowSize += cell.IsNull() ? 1 : cell.Size();
        rowItems[i] = GetCellValue(cell, dbTuple.Types[i]);
    }

    // Some per-row overhead to deal with the case when no columns were requested
    rowSize = std::max(rowSize, (size_t) 8);

    for (ui32 i = dbTuple.ColumnCount, j = 0; i < columnsCount; ++i, ++j) {
        switch (systemColumnTags[j]) {
            case TKeyDesc::EColumnIdDataShard:
                rowItems[i] = TUnboxedValue(TUnboxedValuePod(shardId));
                break;
            default:
                throw TSchemeErrorTabletException();
        }
    }
}

static void BuildRowWideImpl(const TDbTupleRef& dbTuple, const TSmallVec<TTag>& systemColumnTags, ui64 shardId,
    NUdf::TUnboxedValue* const* result, size_t& rowSize)
{
    size_t columnsCount = dbTuple.ColumnCount + systemColumnTags.size();

    rowSize = 0;
    for (ui32 i = 0; i < dbTuple.ColumnCount; ++i) {
        const auto& cell = dbTuple.Cells()[i];
        rowSize += cell.IsNull() ? 1 : cell.Size();
        if (auto out = *result++) {
            *out = GetCellValue(cell, dbTuple.Types[i]);
        }
    }

    // Some per-row overhead to deal with the case when no columns were requested
    rowSize = std::max(rowSize, (size_t) 8);

    for (ui32 i = dbTuple.ColumnCount, j = 0; i < columnsCount; ++i, ++j) {
        auto out = *result++;
        if (!out) {
            continue;
        }

        switch (systemColumnTags[j]) {
            case TKeyDesc::EColumnIdDataShard:
                *out = TUnboxedValue(TUnboxedValuePod(shardId));
                break;
            default:
                throw TSchemeErrorTabletException();
        }
    }
}

bool TKqpDatashardComputeContext::ReadRow(const TTableId& tableId, TArrayRef<const TCell> key,
    const TSmallVec<NTable::TTag>& columnTags, const TSmallVec<NTable::TTag>& systemColumnTags,
    const THolderFactory& holderFactory, NUdf::TUnboxedValue& result, TKqpTableStats& kqpStats)
{
    MKQL_ENSURE_S(Shard);

    auto localTid = Shard->GetLocalTableId(tableId);
    auto tableInfo = Database->GetScheme().GetTableInfo(localTid);
    MKQL_ENSURE_S(tableInfo, "Can not resolve table " << tableId);

    TSmallVec<TRawTypeValue> keyValues;
    ConvertTableKeys(Database->GetScheme(), tableInfo, key, keyValues, /* keyDataBytes */ nullptr);

    if (Y_UNLIKELY(keyValues.size() != tableInfo->KeyColumns.size())) {
        throw TSchemeErrorTabletException();
    }

    TouchTablePoint(tableId, key);
    Shard->GetKeyAccessSampler()->AddSample(tableId, key);

    NTable::TRowState dbRow;
    NTable::TSelectStats stats;
    ui64 flags = EngineHost.GetSettings().DisableByKeyFilter ? (ui64) NTable::NoByKey : 0;
    auto ready = Database->Select(localTid, keyValues, columnTags, dbRow, stats, flags, GetReadVersion());

    kqpStats.NSelectRow = 1;
    kqpStats.InvisibleRowSkips = stats.InvisibleRowSkips;

    switch (ready) {
        case EReady::Page:
            if (auto collector = EngineHost.GetChangeCollector(tableId)) {
                collector->Reset();
            }
            SetTabletNotReady();
            return false;
        case EReady::Gone:
            return false;
        case EReady::Data:
            break;
    };

    MKQL_ENSURE_S(columnTags.size() == dbRow.Size(), "Invalid local db row size.");

    TVector<NScheme::TTypeId> types(columnTags.size());
    for (size_t i = 0; i < columnTags.size(); ++i) {
        types[i] = tableInfo->Columns.at(columnTags[i]).PType;
    }
    auto dbTuple = TDbTupleRef(types.data(), (*dbRow).data(), dbRow.Size());

    size_t rowSize = 0;
    BuildRowImpl(dbTuple, holderFactory, systemColumnTags, Shard->TabletID(), result, rowSize);

    kqpStats.SelectRowRows = 1;
    kqpStats.SelectRowBytes += rowSize;

    return true;
}

TAutoPtr<NTable::TTableIt> TKqpDatashardComputeContext::CreateIterator(const TTableId& tableId, const TTableRange& range,
    const TSmallVec<NTable::TTag>& columnTags)
{
    auto localTid = Shard->GetLocalTableId(tableId);
    auto tableInfo = Database->GetScheme().GetTableInfo(localTid);
    MKQL_ENSURE_S(tableInfo, "Can not resolve table " << tableId);

    TSmallVec<TRawTypeValue> from, to;
    ConvertTableKeys(Database->GetScheme(), tableInfo, range.From, from, /* keyDataBytes */ nullptr);
    ConvertTableKeys(Database->GetScheme(), tableInfo, range.To, to, /* keyDataBytes */ nullptr);

    NTable::TKeyRange keyRange;
    keyRange.MinKey = from;
    keyRange.MaxKey = to;
    keyRange.MinInclusive = range.InclusiveFrom;
    keyRange.MaxInclusive = range.InclusiveTo;

    TouchTableRange(tableId, range);
    return Database->IterateRange(localTid, keyRange, columnTags, GetReadVersion());
}

TAutoPtr<NTable::TTableReverseIt> TKqpDatashardComputeContext::CreateReverseIterator(const TTableId& tableId,
    const TTableRange& range, const TSmallVec<NTable::TTag>& columnTags)
{
    auto localTid = Shard->GetLocalTableId(tableId);
    auto tableInfo = Database->GetScheme().GetTableInfo(localTid);
    MKQL_ENSURE_S(tableInfo, "Can not resolve table " << tableId);

    TSmallVec<TRawTypeValue> from, to;
    ConvertTableKeys(Database->GetScheme(), tableInfo, range.From, from, /* keyDataBytes */ nullptr);
    ConvertTableKeys(Database->GetScheme(), tableInfo, range.To, to, /* keyDataBytes */ nullptr);

    NTable::TKeyRange keyRange;
    keyRange.MinKey = from;
    keyRange.MaxKey = to;
    keyRange.MinInclusive = range.InclusiveFrom;
    keyRange.MaxInclusive = range.InclusiveTo;

    TouchTableRange(tableId, range);
    return Database->IterateRangeReverse(localTid, keyRange, columnTags, GetReadVersion());
}

template <typename TReadTableIterator>
bool TKqpDatashardComputeContext::ReadRowImpl(const TTableId& tableId, TReadTableIterator& iterator,
    const TSmallVec<NTable::TTag>& systemColumnTags, const TSmallVec<bool>& skipNullKeys,
    const THolderFactory& holderFactory, NUdf::TUnboxedValue& result, TKqpTableStats& stats)
{
    while (iterator.Next(NTable::ENext::Data) == NTable::EReady::Data) {
        TDbTupleRef rowKey = iterator.GetKey();
        MKQL_ENSURE_S(skipNullKeys.size() <= rowKey.ColumnCount);

        Shard->GetKeyAccessSampler()->AddSample(tableId, rowKey.Cells());

        bool skipRow = false;
        for (ui32 i = 0; i < skipNullKeys.size(); ++i) {
            if (skipNullKeys[i] && rowKey.Columns[i].IsNull()) {
                skipRow = true;
                break;
            }
        }

        if (skipRow) {
            continue;
        }

        TDbTupleRef rowValues = iterator.GetValues();
        size_t rowSize = 0;

        BuildRowImpl(rowValues, holderFactory, systemColumnTags, Shard->TabletID(), result, rowSize);

        stats.SelectRangeRows = 1;
        stats.SelectRangeBytes = rowSize;

        stats.InvisibleRowSkips = std::exchange(iterator.Stats.InvisibleRowSkips, 0);
        stats.SelectRangeDeletedRowSkips = std::exchange(iterator.Stats.DeletedRowSkips, 0);

        return true;
    }

    if (iterator.Last() == NTable::EReady::Page) {
        if (auto collector = EngineHost.GetChangeCollector(tableId)) {
            collector->Reset();
        }
        SetTabletNotReady();
    }

    return false;
}

template <typename TReadTableIterator>
bool TKqpDatashardComputeContext::ReadRowWideImpl(const TTableId& tableId, TReadTableIterator& iterator,
    const TSmallVec<NTable::TTag>& systemColumnTags, const TSmallVec<bool>& skipNullKeys,
    NUdf::TUnboxedValue* const* result, TKqpTableStats& stats)
{
    while (iterator.Next(NTable::ENext::Data) == NTable::EReady::Data) {
        TDbTupleRef rowKey = iterator.GetKey();
        MKQL_ENSURE_S(skipNullKeys.size() <= rowKey.ColumnCount);

        Shard->GetKeyAccessSampler()->AddSample(tableId, rowKey.Cells());

        bool skipRow = false;
        for (ui32 i = 0; i < skipNullKeys.size(); ++i) {
            if (skipNullKeys[i] && rowKey.Columns[i].IsNull()) {
                skipRow = true;
                break;
            }
        }

        if (skipRow) {
            continue;
        }

        TDbTupleRef rowValues = iterator.GetValues();
        size_t rowSize = 0;

        BuildRowWideImpl(rowValues, systemColumnTags, Shard->TabletID(), result, rowSize);

        stats.SelectRangeRows = 1;
        stats.SelectRangeBytes = rowSize;

        stats.InvisibleRowSkips = std::exchange(iterator.Stats.InvisibleRowSkips, 0);
        stats.SelectRangeDeletedRowSkips = std::exchange(iterator.Stats.DeletedRowSkips, 0);

        return true;
    }

    if (iterator.Last() == NTable::EReady::Page) {
        if (auto collector = EngineHost.GetChangeCollector(tableId)) {
            collector->Reset();
        }
        SetTabletNotReady();
    }

    return false;
}

bool TKqpDatashardComputeContext::ReadRow(const TTableId& tableId, NTable::TTableIt& iterator,
    const TSmallVec<NTable::TTag>& systemColumnTags, const TSmallVec<bool>& skipNullKeys,
    const THolderFactory& holderFactory, NUdf::TUnboxedValue& result, TKqpTableStats& stats)
{
    return ReadRowImpl(tableId, iterator, systemColumnTags, skipNullKeys, holderFactory, result, stats);
}

bool TKqpDatashardComputeContext::ReadRow(const TTableId& tableId, NTable::TTableReverseIt& iterator,
    const TSmallVec<NTable::TTag>& systemColumnTags, const TSmallVec<bool>& skipNullKeys,
    const THolderFactory& holderFactory, NUdf::TUnboxedValue& result, TKqpTableStats& stats)
{
    return ReadRowImpl(tableId, iterator, systemColumnTags, skipNullKeys, holderFactory, result, stats);
}

bool TKqpDatashardComputeContext::ReadRowWide(const TTableId& tableId, NTable::TTableIt& iterator,
    const TSmallVec<NTable::TTag>& systemColumnTags, const TSmallVec<bool>& skipNullKeys,
    NUdf::TUnboxedValue* const* result, TKqpTableStats& stats)
{
    return ReadRowWideImpl(tableId, iterator, systemColumnTags, skipNullKeys,result, stats);
}

bool TKqpDatashardComputeContext::ReadRowWide(const TTableId& tableId, NTable::TTableReverseIt& iterator,
    const TSmallVec<NTable::TTag>& systemColumnTags, const TSmallVec<bool>& skipNullKeys,
    NUdf::TUnboxedValue* const* result, TKqpTableStats& stats)
{
    return ReadRowWideImpl(tableId, iterator, systemColumnTags, skipNullKeys, result, stats);
}

} // namespace NMiniKQL
} // namespace NKikimr
