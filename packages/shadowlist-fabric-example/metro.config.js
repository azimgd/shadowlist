const path = require('path');
const { getDefaultConfig } = require('@react-native/metro-config');
const { withMetroConfig } = require('react-native-monorepo-config');

const monorepoRoot = path.resolve(__dirname, '../..');
const libraryRoot = path.resolve(__dirname, '../shadowlist-fabric');
const utilsRoot = path.resolve(__dirname, '../shadowlist-utils');

/**
 * Metro configuration
 * https://facebook.github.io/metro/docs/configuration
 *
 * @type {import('metro-config').MetroConfig}
 */
const config = withMetroConfig(getDefaultConfig(__dirname), {
  root: libraryRoot,
  dirname: __dirname,
  workspaces: [],
});

config.watchFolders = [...(config.watchFolders || []), utilsRoot, monorepoRoot];

config.resolver = config.resolver || {};
config.resolver.extraNodeModules = {
  ...(config.resolver.extraNodeModules || {}),
  'shadowlist-utils': utilsRoot,
};

config.resolver.nodeModulesPaths = [
  ...(config.resolver.nodeModulesPaths || []),
  path.join(__dirname, 'node_modules'),
  path.join(monorepoRoot, 'node_modules'),
];

module.exports = config;
