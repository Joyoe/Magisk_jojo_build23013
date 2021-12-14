package com.topjohnwu.magisk.events.dialog

import com.topjohnwu.magisk.R
import com.topjohnwu.magisk.core.Info
import com.topjohnwu.magisk.core.download.DownloadService
import com.topjohnwu.magisk.core.download.Subject
import com.topjohnwu.magisk.di.AppContext
import com.topjohnwu.magisk.di.ServiceLocator
import com.topjohnwu.magisk.view.MagiskDialog
import java.io.File

class ManagerInstallDialog : MarkDownDialog() {

    private val svc get() = ServiceLocator.networkService

    override suspend fun getMarkdownText(): String {
        val text = svc.fetchString(Info.remote.magisk.note)
        // Cache the changelog
        AppContext.cacheDir.listFiles { _, name -> name.endsWith(".md") }.orEmpty().forEach {
            it.delete()
        }
        File(AppContext.cacheDir, "${Info.remote.magisk.versionCode}.md").writeText(text)
        return text
    }

    override fun build(dialog: MagiskDialog) {
        super.build(dialog)
        with(dialog) {
            setCancelable(true)
            applyButton(MagiskDialog.ButtonType.POSITIVE) {
                titleRes = R.string.install
                onClick { DownloadService.start(context, Subject.Manager()) }
            }
            applyButton(MagiskDialog.ButtonType.NEGATIVE) {
                titleRes = android.R.string.cancel
            }
        }
    }

}
