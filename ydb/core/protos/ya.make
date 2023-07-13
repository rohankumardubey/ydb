PROTO_LIBRARY()

GRPC()

IF (OS_WINDOWS)
    NO_OPTIMIZE_PY_PROTOS()
ENDIF()

SRCS(
    alloc.proto
    base.proto
    bind_channel_storage_pool.proto
    blob_depot.proto
    blob_depot_config.proto
    blobstorage.proto
    blobstorage_base.proto
    blobstorage_base3.proto
    blobstorage_disk.proto
    blobstorage_disk_color.proto
    blobstorage_pdisk_config.proto
    blobstorage_vdisk_config.proto
    blobstorage_vdisk_internal.proto
    blobstorage_config.proto
    blockstore_config.proto
    filestore_config.proto
    bootstrapper.proto
    change_exchange.proto
    channel_purpose.proto
    cms.proto
    config.proto
    config_units.proto
    console.proto
    console_base.proto
    console_config.proto
    console_tenant.proto
    counters_tx_allocator.proto
    counters_blob_depot.proto
    counters_bs_controller.proto
    counters_cms.proto
    counters_coordinator.proto
    counters_columnshard.proto
    counters_datashard.proto
    counters_hive.proto
    counters_kesus.proto
    counters_keyvalue.proto
    counters_pq.proto
    counters_replication.proto
    counters_schemeshard.proto
    counters_sequenceshard.proto
    counters_sysview_processor.proto
    counters_testshard.proto
    counters_tx_proxy.proto
    counters_mediator.proto
    counters.proto
    database_basic_sausage_metainfo.proto
    datashard_load.proto
    drivemodel.proto
    export.proto
    external_sources.proto
    flat_tx_scheme.proto
    flat_scheme_op.proto
    health.proto
    hive.proto
    http_config.proto
    import.proto
    index_builder.proto
    kesus.proto
    kqp_physical.proto
    kqp_stats.proto
    kqp.proto
    labeled_counters.proto
    load_test.proto
    local.proto
    long_tx_service.proto
    maintenance.proto
    metrics.proto
    minikql_engine.proto
    mon.proto
    msgbus.proto
    msgbus_health.proto
    msgbus_kv.proto
    msgbus_pq.proto
    netclassifier.proto
    node_broker.proto
    node_limits.proto
    profiler.proto
    query_stats.proto
    replication.proto
    resource_broker.proto
    scheme_log.proto
    scheme_type_metadata.proto
    scheme_type_operation.proto
    serverless_proxy_config.proto
    shared_cache.proto
    sqs.proto
    follower_group.proto
    ssa.proto
    statestorage.proto
    stream.proto
    subdomains.proto
    table_stats.proto
    tablet.proto
    tablet_counters.proto
    tablet_counters_aggregator.proto
    tablet_database.proto
    tablet_pipe.proto
    tablet_tracing_signals.proto
    tablet_tx.proto
    tenant_pool.proto
    tenant_slot_broker.proto
    test_shard.proto
    tracing.proto
    node_whiteboard.proto
    tx.proto
    tx_columnshard.proto
    tx_datashard.proto
    tx_mediator_timecast.proto
    tx_proxy.proto
    tx_scheme.proto
    tx_sequenceshard.proto
    pdiskfit.proto
    pqconfig.proto
    auth.proto
    key.proto
    grpc.proto
    grpc_pq_old.proto
    grpc_status_proxy.proto
    ydb_result_set_old.proto
    ydb_table_impl.proto
    scheme_board.proto
    scheme_board_mon.proto
    sys_view.proto
)

GENERATE_ENUM_SERIALIZATION(blobstorage_pdisk_config.pb.h)
GENERATE_ENUM_SERIALIZATION(datashard_load.pb.h)

PEERDIR(
    library/cpp/actors/protos
    ydb/core/fq/libs/config/protos
    ydb/core/scheme/protos
    ydb/library/login/protos
    ydb/library/mkql_proto/protos
    ydb/public/api/protos
    ydb/library/yql/core/issue/protos
    ydb/library/yql/dq/actors/protos
    ydb/library/yql/dq/proto
    ydb/library/yql/public/issue/protos
    ydb/library/yql/public/types
    ydb/library/services
    ydb/library/ydb_issue/proto
)

EXCLUDE_TAGS(GO_PROTO)

END()
