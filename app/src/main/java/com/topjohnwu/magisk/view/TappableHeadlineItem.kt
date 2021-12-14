package com.topjohnwu.magisk.view

import com.topjohnwu.magisk.R
import com.topjohnwu.magisk.databinding.DiffRvItem

sealed class TappableHeadlineItem : DiffRvItem<TappableHeadlineItem>() {

    abstract val title: Int
    abstract val icon: Int

    override val layoutRes = R.layout.item_tappable_headline

    // --- listener

    interface Listener {

        fun onItemPressed(item: TappableHeadlineItem)

    }

    // --- objects

    object Hide : TappableHeadlineItem() {
        override val title = R.string.denylist
        override val icon = R.drawable.ic_hide_md2
    }

    object ThemeMode : TappableHeadlineItem() {
        override val title = R.string.settings_dark_mode_title
        override val icon = R.drawable.ic_day_night
    }

}
