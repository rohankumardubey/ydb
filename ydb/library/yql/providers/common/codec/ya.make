LIBRARY()

OWNER(g:yql spuchin g:yql_ydb_core)

SRCS(
    yql_codec.cpp
    yql_codec.h
    yql_codec_buf.cpp
    yql_codec_buf.h
    yql_codec_results.cpp
    yql_codec_results.h
    yql_restricted_yson.cpp
    yql_restricted_yson.h
    yql_codec_type_flags.cpp 
    yql_codec_type_flags.h 
)

PEERDIR(
    ydb/library/yql/minikql
    ydb/library/yql/minikql/computation
    ydb/library/yql/providers/common/mkql
    library/cpp/yson/node
    library/cpp/yson
)

YQL_LAST_ABI_VERSION() 

GENERATE_ENUM_SERIALIZATION(yql_codec_type_flags.h) 

END()
