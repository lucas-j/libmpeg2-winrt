using System;
using System.Collections.Generic;
using Windows.ApplicationModel;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Navigation;
using Windows.Storage;
using Windows.Storage.AccessCache;
using Windows.Storage.Streams;
using Windows.System.Display;
using Microsoft.PlayerFramework;
using mpeg2_player.Common;
using mpeg2_player.Data;

namespace mpeg2_player {
    public sealed partial class PlayerPage: LayoutAwarePage {
        public static readonly Guid MPEG2_GUID = new Guid("e06d8026-db46-11cf-b4d1-00805f6cbbea");
        private StorageFile file = null;
        private DisplayRequest req = null;
        private bool teardown = false;

        public PlayerPage() {
            this.Loaded += PlayerPage_Loaded;
            this.PointerMoved += PlayerPage_PointerMoved;
            this.InitializeComponent(); }

        private void PlayerPage_Loaded(object sender, RoutedEventArgs e) {
            MediaPlayer.MediaExtensionManager.RegisterVideoDecoder("libmpeg2.Decoder", MPEG2_GUID, new Guid());
            MediaPlayer.MediaStarted += MediaPlayer_MediaStarted;
            MediaPlayer.MediaEnded += MediaPlayer_MediaEnded;
            MediaPlayer.IsInteractiveChanged += MediaPlayer_IsInteractiveChanged; }

        private void PlayerPage_PointerMoved(object sender, PointerRoutedEventArgs e) {
            MediaPlayer.IsInteractive = true; }

        private void MediaPlayer_IsInteractiveChanged(object sender, RoutedEventArgs e) {
 	        if(MediaPlayer.IsInteractive) {
                VisualStateManager.GoToState(this, "Revealing", true);
                App.Current.RestoreCursor(); }
            else {
                VisualStateManager.GoToState(this, "Hiding", true);
                App.Current.HideCursor(); } }

        private void MediaPlayer_MediaStarted(object sender, RoutedEventArgs e) {
            App.Current.AddRecentlyUsed(file);
            if(req == null) {
                req = new DisplayRequest();
                req.RequestActive(); } }

        void MediaPlayer_MediaEnded(object sender, MediaPlayerActionEventArgs e) {
            if(req != null) {
                req.RequestRelease();
                req = null; } }

        private void App_Resuming(object sender, object e) {
            MediaPlayer.PlayResume(); }

        private void App_Suspending(object sender, SuspendingEventArgs e) {
            MediaPlayer.Pause(); }
        
        protected override async void LoadState(object param, Dictionary<string, object> pageState) {
            base.LoadState(param, pageState);
            if(param.GetType() == typeof(string)) {
                /* streaming URL from m3u */
                MediaPlayer.Source = new Uri((string)param, UriKind.Absolute);
                MediaPlayer.Play(); }
            else { 
                int id = (int)param;
                VideoDataItem item = VideoDataSource.GetItem(id);
                if(item != null) {
                    OpenFile(item.File); }
                else if(pageState["fileToken"] != null) {
                    StorageItemAccessList future = StorageApplicationPermissions.FutureAccessList;
                    StorageFile file = await future.GetFileAsync(pageState["fileToken"] as string);
                    future.Clear();
                    OpenFile(file); }
                if(pageState != null) {
                    MediaPlayerState state = pageState["MediaState"] as MediaPlayerState;
                    if(state != null) {
                        MediaPlayer.RestorePlayerState(state); } } } }

        protected override void SaveState(Dictionary<string, object> pageState) {
            if(file != null) {
                MediaPlayerState state = MediaPlayer.GetPlayerState();
                StorageItemAccessList future = StorageApplicationPermissions.FutureAccessList;
                future.Clear();
                string token = future.Add(file);
                pageState.Add("MediaState", state);
                pageState.Add("fileToken", token);
                base.SaveState(pageState);
                if(teardown) {
                    MediaPlayer.Dispose(); } } }

        protected override void OnNavigatedTo(NavigationEventArgs e) {
            base.OnNavigatedTo(e);
            Application.Current.Resuming += App_Resuming;
            Application.Current.Suspending += App_Suspending; }

        protected override void OnNavigatingFrom(NavigatingCancelEventArgs e) {
            teardown = true;
            Application.Current.Resuming -= App_Resuming;
            Application.Current.Suspending -= App_Suspending;
            base.OnNavigatingFrom(e); }

        private async void OpenFile(StorageFile file) {
            if(file != null) {
                this.file = file;
                IRandomAccessStreamWithContentType stream = await file.OpenReadAsync();
                MediaPlayer.Stop();
                MediaPlayer.SetSource(stream, stream.ContentType); } } } }
