Import("env")

# 禁用实际构建
env.Replace(PROGNAME="dummy", BUILDERS={})