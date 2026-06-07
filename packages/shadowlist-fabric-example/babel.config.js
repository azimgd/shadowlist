const path = require('path');
const { getConfig } = require('react-native-builder-bob/babel-config');
const pkg = require('../shadowlist-fabric/package.json');

const root = path.resolve(__dirname, '../shadowlist-fabric');

module.exports = getConfig(
  {
    presets: ['module:@react-native/babel-preset'],
    plugins: ['react-native-worklets/plugin'],
  },
  { root, pkg }
);
