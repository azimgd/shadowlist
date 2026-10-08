const path = require('path');
const fabricRoot = path.resolve(__dirname, '../../packages/shadowlist-fabric');
const pkg = require(path.join(fabricRoot, 'package.json'));

module.exports = {
  project: {
    ios: {
      automaticPodsInstallation: true,
    },
  },
  dependencies: {
    [pkg.name]: {
      root: fabricRoot,
      platforms: {
        // Codegen fails without these, even when empty.
        ios: {},
        android: {},
      },
    },
  },
};
