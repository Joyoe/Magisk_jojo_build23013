package com.topjohnwu.magisk.ui.deny

import android.annotation.SuppressLint
import android.content.pm.PackageManager.MATCH_UNINSTALLED_PACKAGES
import androidx.lifecycle.viewModelScope
import com.topjohnwu.magisk.BR
import com.topjohnwu.magisk.arch.BaseViewModel
import com.topjohnwu.magisk.arch.Queryable
import com.topjohnwu.magisk.databinding.filterableListOf
import com.topjohnwu.magisk.databinding.itemBindingOf
import com.topjohnwu.magisk.di.AppContext
import com.topjohnwu.magisk.utils.Utils
import com.topjohnwu.superuser.Shell
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.*
import java.util.*
import kotlin.collections.ArrayList

class DenyListViewModel : BaseViewModel(), Queryable {

    override val queryDelay = 0L

    var isShowSystem = false
        set(value) {
            field = value
            submitQuery()
        }

    var isShowOS = false
        set(value) {
            field = value
            submitQuery()
        }

    var query = ""
        set(value) {
            field = value
            submitQuery()
        }

    val items = filterableListOf<DenyListRvItem>()
    val itemBinding = itemBindingOf<DenyListRvItem> {
        it.bindExtra(BR.viewModel, this)
    }
    val itemInternalBinding = itemBindingOf<ProcessRvItem> {
        it.bindExtra(BR.viewModel, this)
    }

    @SuppressLint("InlinedApi")
    override fun refresh() = viewModelScope.launch {
        if (!Utils.showSuperUser()) {
            state = State.LOADING_FAILED
            return@launch
        }
        state = State.LOADING
        val (apps, diff) = withContext(Dispatchers.IO) {
            val pm = AppContext.packageManager
            val denyList = Shell.su("magisk --denylist ls").exec().out
                .map { CmdlineListItem(it) }
            val apps = pm.getInstalledApplications(MATCH_UNINSTALLED_PACKAGES).asFlow()
                .filter { AppContext.packageName != it.packageName }
                .map { async { AppProcessInfo(it, pm, denyList) } }
                .map { it.await() }
                .filter { it.processes.isNotEmpty() }
                .map { async { DenyListRvItem(it) } }
                .map { it.await() }
                .toCollection(ArrayList())
            apps.sort()
            apps to items.calculateDiff(apps)
        }
        items.update(apps, diff)
        submitQuery()
    }

    override fun query() {
        items.filter {
            fun filterSystem() = isShowSystem || !it.info.isSystemApp()

            fun filterOS() = (isShowSystem && isShowOS) || it.info.isApp()

            fun filterQuery(): Boolean {
                fun inName() = it.info.label.contains(query, true)
                fun inPackage() = it.info.packageName.contains(query, true)
                fun inProcesses() = it.processes.any { p -> p.process.name.contains(query, true) }
                return inName() || inPackage() || inProcesses()
            }

            (it.isChecked || (filterSystem() && filterOS())) && filterQuery()
        }
        state = State.LOADED
    }
}
