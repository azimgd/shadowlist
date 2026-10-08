const path = require('path');
const { getDefaultConfig, mergeConfig } = require('@react-native/metro-config');

const libraryRoot = path.resolve(__dirname, '../../packages/shadowlist-fabric');
const utilsRoot = path.resolve(__dirname, '../shadowlist-utils');
/*
 * The iOS and Android example, whose fake API, fixtures and queries this app shares.
 */
const exampleSrc = path.resolve(__dirname, '../shadowlist-fabric-example/src');
const macosRoot = path.dirname(
  require.resolve('react-native-macos/package.json')
);

/*
 * Workspace packages consumed as TypeScript sources rather than build output.
 */
const workspacePackages = ['shadowlist', 'shadowlist-utils'];

module.exports = mergeConfig(getDefaultConfig(__dirname), {
  watchFolders: [libraryRoot, utilsRoot, exampleSrc],
  resolver: {
    // A `.macos.*` extension has to be a candidate before one can be picked.
    platforms: [...getDefaultConfig(__dirname).resolver.platforms, 'macos'],
    // Resolve React and the renderer from this app even for linked library sources.
    disableHierarchicalLookup: true,
    nodeModulesPaths: [path.join(__dirname, 'node_modules')],
    /*
     * `nmHoistingLimits` keeps the other workspace packages out of this app's node_modules.
     * Mapping them here is how a bare specifier reaches its directory, where the `exports`
     * map is read. The iOS and Android examples do the same through
     * `react-native-monorepo-config`.
     */
    extraNodeModules: {
      'shadowlist': libraryRoot,
      'shadowlist-utils': utilsRoot,
    },
    resolveRequest(context, moduleName, platform) {
      if (
        moduleName === 'react-native' ||
        moduleName.startsWith('react-native/')
      ) {
        const relative = moduleName.slice('react-native'.length);
        return context.resolveRequest(context, macosRoot + relative, platform);
      }
      // `@example/queries/feed` is `queries/feed` in the shared example sources.
      if (moduleName.startsWith('@example/')) {
        return context.resolveRequest(
          context,
          path.join(exampleSrc, moduleName.slice('@example/'.length)),
          platform
        );
      }
      /*
       * Prefer the `source` entry of a workspace package and its subpaths, the way the other
       * example apps do. This app then runs the TypeScript sources without a build step.
       *
       * The `native` barrel of shadowlist-utils works here because each module that needs
       * reanimated, gesture-handler or safe-area-context has a `.macos` variant without them.
       */
      if (workspacePackages.some((name) => moduleName.startsWith(name))) {
        return context.resolveRequest(
          {
            ...context,
            mainFields: ['source', ...context.mainFields],
            unstable_conditionNames: [
              'source',
              ...context.unstable_conditionNames,
            ],
          },
          moduleName,
          platform
        );
      }
      return context.resolveRequest(context, moduleName, platform);
    },
  },
});
