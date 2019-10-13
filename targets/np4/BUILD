package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "pi_np4",
    srcs = [
        "counter.cpp",
        "device.cpp",
        "device_mgr.cpp",
        "pi_act_prof_imp.cpp",
        "pi_imp.cpp",
        "pi_clone_imp.cpp",
        "pi_counter_imp.cpp",
        "pi_learn_imp.cpp",
        "pi_mc_imp.cpp",
        "pi_meter_imp.cpp",
        "pi_tables_imp.cpp",
        "tables.cpp",
    ],
    hdrs = [
        "counter.h",
        "device.h",
        "device_mgr.h",
        "tables.h",
    ],
    includes = [".", ],
    copts = ["-DUSE_ABSL=1"],
    deps = [
        "//:pihdrs",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "//proto:np4_intel_device_config_cc_proto",
        "//proto/frontend:pifeproto_logger",
        "//proto:piprotoutil",
    ],
)
