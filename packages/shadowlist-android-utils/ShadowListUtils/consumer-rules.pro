# JNI resolves these parser/step transport types and constructors by their binary names.
-keep,allowoptimization class com.shadowlist.utils.ShadowListUtilsCore { native <methods>; }
-keep,allowoptimization class com.shadowlist.utils.ShadowListUtilsMarkdownInline { public <init>(...); }
-keep,allowoptimization class com.shadowlist.utils.ShadowListUtilsMarkdownBlock { public <init>(...); }
-keep,allowoptimization interface com.shadowlist.utils.ShadowListUtilsAssistantStep
-keep,allowoptimization class com.shadowlist.utils.ShadowListUtilsAssistantStep$Text { public <init>(...); }
-keep,allowoptimization class com.shadowlist.utils.ShadowListUtilsAssistantStep$Calls { public <init>(...); }
