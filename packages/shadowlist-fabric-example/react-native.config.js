const path = require('path');
const pkg = require('../shadowlist-fabric/package.json');

module.exports = {
  project: {
    ios: {
      automaticPodsInstallation: true,
    },
  },
  dependencies: {
    [pkg.name]: {
      root: path.join(__dirname, '../shadowlist-fabric'),
      platforms: {
        // Codegen fails without these, even when empty.
        ios: {},
        android: {},
      },
    },
  },
};
