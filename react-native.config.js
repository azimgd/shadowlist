/**
 * @type {import('@react-native-community/cli-types').UserDependencyConfig}
 */
module.exports = {
  dependency: {
    platforms: {
      android: {
        componentDescriptors: [
          'SLContainerComponentDescriptor',
          'SLContentComponentDescriptor',
          'SLElementComponentDescriptor',
        ],
        cmakeListsPath: '../android/shadowlist/jni/CMakeLists.txt',
      },
    },
  },
};
