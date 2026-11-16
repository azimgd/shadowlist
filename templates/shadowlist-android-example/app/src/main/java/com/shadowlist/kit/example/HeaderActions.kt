package com.shadowlist.kit.example

/*
 * An action of a screen's More menu. A checked action shows a check mark.
 */
class HeaderAction(val label: String, val checked: Boolean? = null, val onPress: () -> Unit)
