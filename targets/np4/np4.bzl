# This Skylark rule imports the DPDK shared libraries and headers. The
# RTE_SDK environment variable needs to be set, otherwise the PI rules
# for the NP4 Implementation cannot be built.

def _impl(repository_ctx):
    if "RTE_SDK" not in repository_ctx.os.environ:
        repository_ctx.file("BUILD", """
""")
        return
    dpdk_path = repository_ctx.os.environ["RTE_SDK"] + "x86_64-native-linux-gcc"
    repository_ctx.symlink(dpdk_path, "dpdk-bin")
    repository_ctx.file("BUILD", """

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "dpdk_hdrs",
    hdrs = glob(["dpdk-bin/include/**/*.h"]),
    includes = ["dpdk-bin/include"],
)

cc_import(
    name = "dpdk",
    hdrs = [], # see cc_library rule above
    static_library = "dpdk-bin/lib/libdpdk.a",
)

cc_import(
    name = "dpdk_eal",
    hdrs = [], # see cc_library rule above
    static_library = "dpdk-bin/lib/librte_eal.a",
)

cc_import(
    name = "dpdk_ethdev",
    hdrs = [], # see cc_library rule above
    static_library = "dpdk-bin/lib/librte_ethdev.a",
)

cc_import(
    name = "dpdk_mbuf",
    hdrs = [], # see cc_library rule above
    static_library = "dpdk-bin/lib/librte_mbuf.a",
)

cc_import(
    name = "dpdk_mempool",
    hdrs = [], # see cc_library rule above
    static_library = "dpdk-bin/lib/librte_mempool.a",
)

cc_import(
    name = "dpdk_kvargs",
    hdrs = [], # see cc_library rule above
    static_library = "dpdk-bin/lib/librte_kvargs.a",
)

""")

np4_configure = repository_rule(
    implementation=_impl,
    local = True,
    environ = ["RTE_SDK"])
