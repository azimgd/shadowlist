const path = require('path');
const { getConfig } = require('react-native-builder-bob/babel-config');

const root = path.resolve(__dirname, '../../packages/shadowlist-fabric');
const pkg = require(path.join(root, 'package.json'));

module.exports = getConfig(
  {
    presets: ['module:@react-native/babel-preset'],
    plugins: ['react-native-worklets/plugin'],
  },
  { root, pkg }
);
