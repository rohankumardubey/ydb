LIBRARY() 
 
OWNER(
    g:yql
    g:yql_ydb_core
)
 
SRCS( 
    yql_mkql_schema.cpp 
) 
 
PEERDIR( 
    library/cpp/yson
    library/cpp/yson/node
    ydb/library/yql/minikql
    ydb/library/yql/public/udf
    ydb/library/yql/utils
    ydb/library/yql/providers/common/codec
    ydb/library/yql/providers/common/schema/parser
) 
 
YQL_LAST_ABI_VERSION() 
 
END() 
