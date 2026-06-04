const path = require('path');
const { getDefaultConfig } = require('@react-native/metro-config');
const { withMetroConfig } = require('react-native-monorepo-config');

const root = path.resolve(__dirname, '..');

// Shared example helpers live in a sibling package outside this workspace, so
// metro needs to watch the folder and resolve the bare specifier to its source.
const utilsRoot = path.resolve(__dirname, '../../shadowlist-utils');

/**
 * Metro configuration
 * https://facebook.github.io/metro/docs/configuration
 *
 * @type {import('metro-config').MetroConfig}
 */
const config = withMetroConfig(getDefaultConfig(__dirname), {
  root,
  dirname: __dirname,
});

config.watchFolders = [...(config.watchFolders || []), utilsRoot];
config.resolver = config.resolver || {};
config.resolver.extraNodeModules = {
  ...(config.resolver.extraNodeModules || {}),
  'shadowlist-utils': utilsRoot,
};

module.exports = config;
