const path = require('path');
const fabricRoot = path.resolve(__dirname, '../../packages/shadowlist-fabric');

module.exports = {
  reactNativePath: path.dirname(
    require.resolve('react-native-macos/package.json')
  ),
  dependencies: {
    shadowlist: { root: fabricRoot },
  },
};
