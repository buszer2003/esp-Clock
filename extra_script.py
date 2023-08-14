Import("env")

env.Replace(PROGNAME="firmware_v%s" % env.GetProjectOption("custom_prog_version"))