package com.topjohnwu.magisk.events

import android.content.ActivityNotFoundException
import android.content.Context
import android.net.Uri
import android.view.View
import android.widget.Toast
import androidx.navigation.NavDirections
import com.topjohnwu.magisk.MainDirections
import com.topjohnwu.magisk.R
import com.topjohnwu.magisk.arch.*
import com.topjohnwu.magisk.core.Const
import com.topjohnwu.magisk.core.model.module.OnlineModule
import com.topjohnwu.magisk.events.dialog.MarkDownDialog
import com.topjohnwu.magisk.utils.Utils
import com.topjohnwu.magisk.view.MagiskDialog
import com.topjohnwu.magisk.view.Shortcuts

class OpenReadmeEvent(private val item: OnlineModule) : MarkDownDialog() {
    override suspend fun getMarkdownText() = item.notes()
    override fun build(dialog: MagiskDialog) {
        super.build(dialog)
        dialog.applyButton(MagiskDialog.ButtonType.NEGATIVE) {
            titleRes = android.R.string.cancel
        }.cancellable(true)
    }
}

class PermissionEvent(
    private val permission: String,
    private val callback: (Boolean) -> Unit
) : ViewEvent(), ActivityExecutor {

    override fun invoke(activity: BaseUIActivity<*, *>) =
        activity.withPermission(permission, callback)
}

class BackPressEvent : ViewEvent(), ActivityExecutor {
    override fun invoke(activity: BaseUIActivity<*, *>) {
        activity.onBackPressed()
    }
}

class DieEvent : ViewEvent(), ActivityExecutor {
    override fun invoke(activity: BaseUIActivity<*, *>) {
        activity.finish()
    }
}

class ShowUIEvent(private val delegate: View.AccessibilityDelegate?)
    : ViewEvent(), ActivityExecutor {
    override fun invoke(activity: BaseUIActivity<*, *>) {
        activity.setContentView()
        activity.setAccessibilityDelegate(delegate)
    }
}

class RecreateEvent : ViewEvent(), ActivityExecutor {
    override fun invoke(activity: BaseUIActivity<*, *>) {
        activity.recreate()
    }
}

class MagiskInstallFileEvent(
    private val callback: (Uri) -> Unit
) : ViewEvent(), ActivityExecutor {
    override fun invoke(activity: BaseUIActivity<*, *>) {
        try {
            activity.getContent("*/*", callback)
            Utils.toast(R.string.patch_file_msg, Toast.LENGTH_LONG)
        } catch (e: ActivityNotFoundException) {
            Utils.toast(R.string.app_not_found, Toast.LENGTH_SHORT)
        }
    }
}

class NavigationEvent(
    private val directions: NavDirections
) : ViewEvent(), ActivityExecutor {
    override fun invoke(activity: BaseUIActivity<*, *>) {
        (activity as? BaseUIActivity<*, *>)?.apply {
            directions.navigate()
        }
    }
}

class AddHomeIconEvent : ViewEvent(), ContextExecutor {
    override fun invoke(context: Context) {
        Shortcuts.addHomeIcon(context)
    }
}

class SelectModuleEvent : ViewEvent(), FragmentExecutor {
    override fun invoke(fragment: BaseUIFragment<*, *>) {
        try {
            fragment.apply {
                activity.getContent("application/zip") {
                    MainDirections.actionFlashFragment(Const.Value.FLASH_ZIP, it).navigate()
                }
            }
        } catch (e: ActivityNotFoundException) {
            Utils.toast(R.string.app_not_found, Toast.LENGTH_SHORT)
        }
    }
}
