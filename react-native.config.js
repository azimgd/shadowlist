/**
 * @type {import('@react-native-community/cli-types').UserDependencyConfig}
 */
module.exports = {
  dependency: {
    platforms: {
      android: {
        componentDescriptors: ['SLContainerComponentDescriptor'],
        cmakeListsPath: '../cpp/CMakeLists.txt',
      },
    },
  },
};
