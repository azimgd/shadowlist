module.exports = {
  dependency: {
    platforms: {
      android: {
        componentDescriptors: [
          'ShadowListContainerComponentDescriptor',
          // 'ShadowListItemComponentDescriptor',
        ],
        cmakeListsPath: '../cpp/CMakeLists.txt',
      },
    },
  },
};
