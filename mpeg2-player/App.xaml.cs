using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using Windows.ApplicationModel;
using Windows.ApplicationModel.Activation;
using Windows.Storage;
using Windows.Storage.AccessCache;
using Windows.Storage.FileProperties;
using Windows.Storage.Pickers;
using Windows.System;
using Windows.UI;
using Windows.UI.ApplicationSettings;
using Windows.UI.Core;
using Windows.UI.Popups;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media.Imaging;
using Windows.UI.ViewManagement;
using Callisto.Controls;
using mpeg2_player.Common;
using mpeg2_player.Data;

namespace mpeg2_player {
    sealed partial class App: Application {
        public static readonly IEnumerable<string> VideoExtensions = new ReadOnlyCollection<string>(
            new List<string>() { ".mpg", ".mpeg", ".m2v", ".ts" });
        new public static App Current;
        private CoreCursor cursor = null;

        public App() {
            Current = this;
            this.InitializeComponent();
            this.Suspending += OnSuspending; }

        static App() {
            SuspensionManager.KnownTypes.Add(typeof(Microsoft.PlayerFramework.MediaPlayerState)); }

        protected override async void OnLaunched(LaunchActivatedEventArgs args) {
            Frame rootFrame = Window.Current.Content as Frame;
            if(rootFrame == null) {
                rootFrame = new Frame();
                SuspensionManager.RegisterFrame(rootFrame, "AppFrame");
                if(args.PreviousExecutionState == ApplicationExecutionState.Terminated) {
                    try {
                        await SuspensionManager.RestoreAsync(); }
                    catch (SuspensionManagerException) {} }
                Window.Current.Content = rootFrame; }
            if(rootFrame.Content == null) {
                if(!rootFrame.Navigate(typeof(MainPage))) {
                    throw new Exception("Failed to create initial page"); } }
            Window.Current.Activate();
            cursor = Window.Current.CoreWindow.PointerCursor;
            PopulateRecentlyUsed();
            PopulateLibrary();
            SettingsPane.GetForCurrentView().CommandsRequested += CommandsRequested; }

        private async void OnSuspending(object sender, SuspendingEventArgs e) {
            SuspendingDeferral deferral = e.SuspendingOperation.GetDeferral();
            await SuspensionManager.SaveAsync();
            deferral.Complete(); }

        private void CommandsRequested(SettingsPane sender, SettingsPaneCommandsRequestedEventArgs args) {
            SettingsCommand about = new SettingsCommand("About", "About", About_Click);
            SettingsCommand privacy = new SettingsCommand("Privacy", "Privacy Policy", PrivacyPolicy_Click);
            SettingsCommand history = new SettingsCommand("History", "Clear History", ClearHistory_Click);
            args.Request.ApplicationCommands.Add(about);
            args.Request.ApplicationCommands.Add(privacy);
            args.Request.ApplicationCommands.Add(history); }

        private void About_Click(IUICommand cmd) {
            SettingsFlyout settings = new SettingsFlyout();
            settings.Background = new SolidColorBrush(Colors.White);
            settings.HeaderBrush = new SolidColorBrush(Colors.Black);
            settings.Content = new AboutPanel();
            settings.HeaderText = "About";
            settings.IsOpen = true;
            RestoreCursor(); }

        private async void PrivacyPolicy_Click(IUICommand cmd) {
            await Launcher.LaunchUriAsync(new Uri("http://vidya.dyndns.org/mpeg2player.html")); }

        private void ClearHistory_Click(IUICommand cmd) {
            App.Current.ClearRecentlyUsed(); }

        private async void PopulateRecentlyUsed() {
            StorageItemMostRecentlyUsedList mru = StorageApplicationPermissions.MostRecentlyUsedList;
            foreach(AccessListEntry item in mru.Entries) {
                StorageFile file = await mru.GetFileAsync(item.Token);
                AddFile(file, 0); } }

        private async void PopulateLibrary() {
            StorageFolder videos = KnownFolders.VideosLibrary;
            IReadOnlyList<StorageFile> files = await videos.GetFilesAsync();
            foreach(StorageFile file in files) {
                AddFile(file, 1); }
            IReadOnlyList<StorageFolder> dirs = await videos.GetFoldersAsync();
            foreach(StorageFolder dir in dirs) {
                int id = VideoDataSource.AddGroup(dir.DisplayName);
                files = await dir.GetFilesAsync();
                foreach(StorageFile file in files) {
                    AddFile(file, id); } } }

        public int AddFile(StorageFile file, int groupID) {
            if(file != null && VideoExtensions.Contains(file.FileType)) {
                IEnumerable<VideoDataItem> matches = VideoDataSource.GetGroup(groupID).Items.Where(itm => itm.Path.Equals(file.Path));
                if(matches.Count() > 0)
                    return matches.First().ID;
                VideoDataItem item = new VideoDataItem(VideoDataSource.NextID(), String.Empty, String.Empty, file);
                VideoDataSource.GetGroup(groupID).Items.Add(item);
                item.SetImage("ms-appx:///Assets/Thumbnail.png");
                AddThumbnail(file, item);
                return item.ID; }
            return -1; }

        public async void AddThumbnail(StorageFile file, VideoDataItem item) {
            StorageItemThumbnail thumb = await file.GetThumbnailAsync(ThumbnailMode.VideosView);
            if(thumb != null) {
                BitmapImage img = new BitmapImage();
                await img.SetSourceAsync(thumb);
                item.Image = img; } }

        public async Task<int> PickFile() {
            if(ApplicationView.Value != ApplicationViewState.Snapped || ApplicationView.TryUnsnap()) {
                FileOpenPicker picker = new FileOpenPicker();
                picker.SuggestedStartLocation = PickerLocationId.VideosLibrary;
                foreach(string ext in VideoExtensions) {
                    picker.FileTypeFilter.Add(ext); }
                StorageFile file = await picker.PickSingleFileAsync();
                return AddFile(file, 0); }
            return -1; }

        public void HideCursor() {
            Window.Current.CoreWindow.PointerCursor = null; }

        public void RestoreCursor() {
            Window.Current.CoreWindow.PointerCursor = cursor; }

        public void AddRecentlyUsed(StorageFile file) {
            if(file != null) {
                StorageApplicationPermissions.MostRecentlyUsedList.Add(file);
                AddFile(file, 0); } }

        public void ClearRecentlyUsed() {
            StorageApplicationPermissions.MostRecentlyUsedList.Clear();
            VideoDataSource.GetGroup(0).Items.Clear(); } } }
