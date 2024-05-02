module.exports = {
  dependency: {
    platforms: {
      android: {
        componentDescriptors: [
          'ShadowListContainerComponentDescriptor',
          'ShadowListItemComponentDescriptor',
        ],
        cmakeListsPath: '../jni/CMakeLists.txt',
      },
    },
  },
};
