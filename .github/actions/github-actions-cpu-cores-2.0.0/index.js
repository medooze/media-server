const os = require("os");
const core = require("@actions/core");

const numberOfCpus =
  typeof os.availableParallelism === "function"
    ? os.availableParallelism()
    : os.cpus().length;

core.setOutput("count", numberOfCpus);
