#include "ui/addons/addons_tab.hpp"

#include "data/addons/addon_repository.hpp"
#include "ui/addons/addon_cell.hpp"

using namespace brls::literals; // for _i18n
using nuvio::data::addons::AddonRepository;
using Status = AddonRepository::InstallStatus;

namespace
{
// Long enough for configured add-on URLs, which carry their settings in the path.
constexpr int MAX_URL_LENGTH = 1024;

AddonRepository& repo()
{
    return AddonRepository::shared();
}

class AddonsDataSource : public brls::RecyclerDataSource
{
  public:
    explicit AddonsDataSource(AddonsTab* tab) : tab(tab) { }

    int numberOfSections(brls::RecyclerFrame*) override { return 2; }

    int numberOfRows(brls::RecyclerFrame*, int section) override
    {
        if (section == AddonsTab::ACTIONS)
            return 2;
        // one row for the empty-list note
        return std::max<int>(1, repo().addons().size());
    }

    std::string titleForHeader(brls::RecyclerFrame*, int section) override
    {
        return section == AddonsTab::INSTALLED ? "app/addons/installed_header"_i18n : "";
    }

    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override
    {
        const auto& addons = repo().addons();
        if (index.section == AddonsTab::ACTIONS || addons.empty())
        {
            auto* cell = static_cast<AddonActionCell*>(recycler->dequeueReusableCell("Action"));
            if (index.section == AddonsTab::INSTALLED)
                cell->setTitle("app/addons/empty"_i18n);
            else if (index.row == AddonsTab::INSTALL)
                cell->setTitle(tab->isInstalling() ? "app/addons/installing"_i18n : "app/addons/install"_i18n);
            else
                cell->setTitle(repo().isRefreshing() ? "app/addons/refreshing"_i18n : "app/addons/refresh"_i18n);
            return cell;
        }

        auto* cell        = static_cast<AddonCell*>(recycler->dequeueReusableCell("Addon"));
        const auto& addon = addons[index.row];
        cell->setAddon(addon, repo().isLoading(addon.baseUrl));
        cell->onToggle = [this](AddonCell* c) { tab->toggle(c->getIndexPath().row); };
        cell->onMove   = [this](AddonCell* c, int delta) { tab->move(c->getIndexPath().row, delta); };
        return cell;
    }

    void didSelectRowAt(brls::RecyclerFrame*, brls::IndexPath index) override
    {
        if (index.section == AddonsTab::ACTIONS)
        {
            if (index.row == AddonsTab::INSTALL)
                tab->openInstall();
            else
                tab->refresh();
        }
        else if (!repo().addons().empty())
        {
            tab->openActions(index.row);
        }
    }

  private:
    AddonsTab* tab;
};
} // namespace

AddonsTab::AddonsTab()
{
    this->inflateFromXMLRes("xml/tabs/addons.xml");

    recycler->estimatedRowHeight = 100;
    recycler->registerCell("Action", AddonActionCell::create);
    recycler->registerCell("Addon", AddonCell::create);
    recycler->setDataSource(new AddonsDataSource(this));

    subscription = repo().subscribe([this]
        {
            int row        = pendingFocusRow;
            pendingFocusRow = -1;
            reload(row);
        });
}

AddonsTab::~AddonsTab()
{
    if (AddonRepository::hasShared())
        repo().unsubscribe(subscription);
}

brls::View* AddonsTab::create()
{
    return new AddonsTab();
}

bool AddonsTab::hasFocusInside() const
{
    for (brls::View* view = brls::Application::getCurrentFocus(); view; view = view->getParent())
        if (view == this)
            return true;
    return false;
}

void AddonsTab::reload(int focusRow)
{
    bool focused = hasFocusInside();
    brls::IndexPath target(INSTALLED, 0);
    if (focusRow >= 0)
        target = brls::IndexPath(INSTALLED, focusRow);
    else if (auto* cell = dynamic_cast<brls::RecyclerCell*>(brls::Application::getCurrentFocus()); cell && focused)
        target = cell->getIndexPath();

    if (target.section == INSTALLED)
    {
        int last   = std::max<int>(0, static_cast<int>(repo().addons().size()) - 1);
        target.row = std::min(target.row, last);
        target.item = target.row;
    }

    recycler->setDefaultCellFocus(target);
    recycler->reloadData();
    // the focused cell may have been recycled into another row
    if (focused)
        brls::Application::giveFocus(recycler);
}

void AddonsTab::openInstall()
{
    if (installing)
        return;
    brls::Application::getImeManager()->openForText(
        [this](std::string text)
        {
            if (text.empty())
                return;
            installing = true;
            reload();
            repo().install(text, [this](AddonRepository::InstallResult result)
                {
                    installing = false;
                    switch (result.status)
                    {
                        case Status::Installed:
                            brls::Application::notify(brls::getStr("app/addons/installed", result.name));
                            break;
                        case Status::InvalidUrl:
                            brls::Application::notify("app/addons/error_invalid_scheme"_i18n);
                            break;
                        case Status::AlreadyInstalled:
                            brls::Application::notify("app/addons/already_installed"_i18n);
                            break;
                        case Status::FetchFailed:
                            brls::Application::notify(brls::getStr("app/addons/error_fetch", result.detail));
                            break;
                        case Status::InvalidManifest:
                            brls::Application::notify("app/addons/error_manifest"_i18n);
                            break;
                    }
                    // the repository notifies before calling back, while installing was still set,
                    // so the install row needs this reload even after a success
                    reload();
                });
        },
        "app/addons/install"_i18n, "app/addons/install_hint"_i18n, MAX_URL_LENGTH, "https://");
}

void AddonsTab::refresh()
{
    bool started = repo().refreshAll([](int failed)
        {
            if (failed == 0)
                brls::Application::notify("app/addons/refreshed"_i18n);
            else
                brls::Application::notify(brls::getStr("app/addons/refresh_failed", failed));
        });
    if (started)
        reload();
}

void AddonsTab::openActions(size_t index)
{
    const auto& addons = repo().addons();
    if (index >= addons.size())
        return;
    std::string url = addons[index].baseUrl;

    enum Choice
    {
        TOGGLE = 0,
        UP,
        DOWN,
        REMOVE,
    };
    std::vector<std::string> options{
        addons[index].enabled ? "app/addons/disable"_i18n : "app/addons/enable"_i18n,
        "app/addons/move_up"_i18n,
        "app/addons/move_down"_i18n,
        "app/addons/remove"_i18n,
    };

    // act once the dropdown has closed and focus is back in the list, since
    // the change reloads the cells the focus would otherwise return to
    auto chosen   = std::make_shared<int>(-1);
    auto* dropdown = new brls::Dropdown(
        addons[index].displayName, options, [chosen](int selected) { *chosen = selected; }, 0,
        [this, chosen, url](int)
        {
            // the list may have changed while the menu was open
            const auto& current = repo().addons();
            size_t i            = 0;
            while (i < current.size() && current[i].baseUrl != url)
                i++;
            if (i == current.size())
                return;

            switch (*chosen)
            {
                case TOGGLE: toggle(i); break;
                case UP: move(i, -1); break;
                case DOWN: move(i, 1); break;
                case REMOVE: confirmRemove(i); break;
                default: break;
            }
        });
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void AddonsTab::toggle(size_t index)
{
    const auto& addons = repo().addons();
    if (index < addons.size())
        repo().setEnabled(addons[index].baseUrl, !addons[index].enabled);
}

void AddonsTab::move(size_t index, int delta)
{
    // focus follows the moved add-on when the change notification reloads the list
    pendingFocusRow = static_cast<int>(index) + delta;
    if (!repo().move(index, delta))
        pendingFocusRow = -1;
}

void AddonsTab::confirmRemove(size_t index)
{
    const auto& addons = repo().addons();
    if (index >= addons.size())
        return;
    std::string url = addons[index].baseUrl;

    auto* dialog = new brls::Dialog(brls::getStr("app/addons/remove_confirm", addons[index].displayName));
    dialog->addButton("hints/cancel"_i18n, [] {});
    dialog->addButton("app/addons/remove"_i18n, [url] { repo().remove(url); });
    dialog->open();
}
