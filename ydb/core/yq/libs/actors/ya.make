OWNER(g:yq)

LIBRARY()

SRCS(
    clusters_from_connections.cpp
    database_resolver.cpp
    error.cpp
    nodes_health_check.cpp
    nodes_manager.cpp
    pending_fetcher.cpp
    pinger.cpp
    proxy.cpp
    proxy_private.cpp
    result_writer.cpp
    run_actor.cpp
    run_actor_params.cpp
    system_clusters.cpp
    table_bindings_from_bindings.cpp
    task_get.cpp
    task_ping.cpp
    task_result_write.cpp
)

PEERDIR(
    library/cpp/actors/core
    library/cpp/actors/interconnect
    library/cpp/json/yson 
    library/cpp/monlib/dynamic_counters 
    library/cpp/random_provider 
    library/cpp/time_provider 
    library/cpp/yson 
    library/cpp/yson/node 
    ydb/core/base
    ydb/core/protos
    ydb/core/yq/libs/actors/logging
    ydb/core/yq/libs/checkpointing
    ydb/core/yq/libs/checkpointing_common
    ydb/core/yq/libs/common
    ydb/core/yq/libs/control_plane_storage
    ydb/core/yq/libs/control_plane_storage/events
    ydb/core/yq/libs/db_resolver
    ydb/core/yq/libs/db_schema
    ydb/core/yq/libs/events
    ydb/core/yq/libs/private_client
    ydb/core/yq/libs/result_formatter
    ydb/core/yq/libs/shared_resources
    ydb/core/yq/libs/signer
    ydb/library/mkql_proto
    ydb/library/security
    ydb/library/yql/ast
    ydb/library/yql/core/facade
    ydb/library/yql/core/services/mounts
    ydb/library/yql/minikql
    ydb/library/yql/minikql/comp_nodes
    ydb/library/yql/providers/common/token_accessor/client
    ydb/library/yql/public/issue
    ydb/library/yql/public/issue/protos
    ydb/library/yql/sql/settings
    ydb/library/yql/utils/actor_log
    ydb/public/api/protos
    ydb/public/lib/yq
    ydb/public/sdk/cpp/client/ydb_table
    ydb/library/yql/providers/clickhouse/provider
    ydb/library/yql/providers/common/codec
    ydb/library/yql/providers/common/comp_nodes
    ydb/library/yql/providers/common/provider
    ydb/library/yql/providers/common/schema/mkql
    ydb/library/yql/providers/common/udf_resolve
    ydb/library/yql/providers/dq/actors
    ydb/library/yql/providers/dq/common
    ydb/library/yql/providers/dq/counters
    ydb/library/yql/providers/dq/interface
    ydb/library/yql/providers/dq/provider
    ydb/library/yql/providers/dq/provider/exec
    ydb/library/yql/providers/dq/worker_manager/interface
    ydb/library/yql/providers/pq/cm_client/interface
    ydb/library/yql/providers/pq/provider
    ydb/library/yql/providers/pq/task_meta
    ydb/library/yql/providers/s3/provider
    ydb/library/yql/providers/ydb/provider
)

YQL_LAST_ABI_VERSION()

END()

RECURSE(
    logging
)
