package com.topjohnwu.magisk.ui.deny

import android.content.Context
import android.os.Bundle
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import androidx.appcompat.widget.SearchView
import androidx.recyclerview.widget.RecyclerView
import com.topjohnwu.magisk.R
import com.topjohnwu.magisk.arch.BaseUIFragment
import com.topjohnwu.magisk.databinding.FragmentDenyMd2Binding
import com.topjohnwu.magisk.di.viewModel
import com.topjohnwu.magisk.ktx.addSimpleItemDecoration
import com.topjohnwu.magisk.ktx.addVerticalPadding
import com.topjohnwu.magisk.ktx.fixEdgeEffect
import com.topjohnwu.magisk.ktx.hideKeyboard

class DenyListFragment : BaseUIFragment<DenyListViewModel, FragmentDenyMd2Binding>() {

    override val layoutRes = R.layout.fragment_deny_md2
    override val viewModel by viewModel<DenyListViewModel>()

    private lateinit var searchView: SearchView

    override fun onAttach(context: Context) {
        super.onAttach(context)
        activity.setTitle(R.string.denylist)
        setHasOptionsMenu(true)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.appList.addOnScrollListener(object : RecyclerView.OnScrollListener() {
            override fun onScrollStateChanged(recyclerView: RecyclerView, newState: Int) {
                if (newState != RecyclerView.SCROLL_STATE_IDLE) hideKeyboard()
            }
        })

        val resource = requireContext().resources
        val l_50 = resource.getDimensionPixelSize(R.dimen.l_50)
        val l1 = resource.getDimensionPixelSize(R.dimen.l1)
        binding.appList.addVerticalPadding(
            l_50,
            l1 + resource.getDimensionPixelSize(R.dimen.internal_action_bar_size)
        )
        binding.appList.addSimpleItemDecoration(
            left = l1,
            top = l_50,
            right = l1,
            bottom = l_50,
        )
        binding.appList.fixEdgeEffect()
    }

    override fun onPreBind(binding: FragmentDenyMd2Binding) = Unit

    override fun onBackPressed(): Boolean {
        if (searchView.isIconfiedByDefault && !searchView.isIconified) {
            searchView.isIconified = true
            return true
        }
        return super.onBackPressed()
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.menu_deny_md2, menu)
        searchView = menu.findItem(R.id.action_search).actionView as SearchView
        searchView.queryHint = searchView.context.getString(R.string.hide_filter_hint)
        searchView.setOnQueryTextListener(object : SearchView.OnQueryTextListener {
            override fun onQueryTextSubmit(query: String?): Boolean {
                viewModel.query = query ?: ""
                return true
            }

            override fun onQueryTextChange(newText: String?): Boolean {
                viewModel.query = newText ?: ""
                return true
            }
        })
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId) {
            R.id.action_show_system -> {
                val check = !item.isChecked
                viewModel.isShowSystem = check
                item.isChecked = check
                return true
            }
            R.id.action_show_OS -> {
                val check = !item.isChecked
                viewModel.isShowOS = check
                item.isChecked = check
                return true
            }
        }
        return super.onOptionsItemSelected(item)
    }

    override fun onPrepareOptionsMenu(menu: Menu) {
        val showSystem = menu.findItem(R.id.action_show_system)
        val showOS = menu.findItem(R.id.action_show_OS)
        showOS.isEnabled = showSystem.isChecked
    }
}
