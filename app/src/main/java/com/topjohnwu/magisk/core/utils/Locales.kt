@file:Suppress("DEPRECATION")

package com.topjohnwu.magisk.core.utils

import android.annotation.SuppressLint
import android.content.res.Configuration
import android.content.res.Resources
import com.topjohnwu.magisk.R
import com.topjohnwu.magisk.core.AssetHack
import com.topjohnwu.magisk.core.Config
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.util.*
import kotlin.collections.ArrayList

var currentLocale: Locale = Locale.getDefault()

@SuppressLint("ConstantLocale")
val defaultLocale: Locale = Locale.getDefault()

private var cachedLocales: Pair<Array<String>, Array<String>>? = null

suspend fun availableLocales() = cachedLocales ?:
withContext(Dispatchers.Default) {
    val compareId = R.string.app_changelog

    // Create a completely new resource to prevent cross talk over active configs
    val res = AssetHack.newResource()

    val locales = ArrayList<String>().apply {
        // Add default locale
        add("en")

        // Add some special locales
        add("zh-TW")
        add("pt-BR")

        // Then add all supported locales
        addAll(Resources.getSystem().assets.locales)
    }.map {
        Locale.forLanguageTag(it)
    }.distinctBy {
        res.updateLocale(it)
        res.getString(compareId)
    }.sortedWith { a, b ->
        a.getDisplayName(a).compareTo(b.getDisplayName(b), true)
    }

    res.updateLocale(defaultLocale)
    val defName = res.getString(R.string.system_default)

    val names = ArrayList<String>(locales.size + 1)
    val values = ArrayList<String>(locales.size + 1)

    names.add(defName)
    values.add("")

    locales.forEach { locale ->
        names.add(locale.getDisplayName(locale))
        values.add(locale.toLanguageTag())
    }

    (names.toTypedArray() to values.toTypedArray()).also { cachedLocales = it }
}

fun Resources.updateConfig(config: Configuration = configuration) {
    config.setLocale(currentLocale)
    updateConfiguration(config, displayMetrics)
}

fun Resources.updateLocale(locale: Locale) {
    configuration.setLocale(locale)
    updateConfiguration(configuration, displayMetrics)
}

fun refreshLocale() {
    val localeConfig = Config.locale
    currentLocale = when {
        localeConfig.isEmpty() -> defaultLocale
        else -> Locale.forLanguageTag(localeConfig)
    }
    Locale.setDefault(currentLocale)
    AssetHack.resource.updateConfig()
}
