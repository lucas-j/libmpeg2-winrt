using System;
using System.Linq;
using Windows.UI.Xaml.Media;

namespace Microsoft.PlayerFramework
{
    public static class MediaPlayerExtensions {
        public static MediaPlayerState GetPlayerState(this MediaPlayer player) {
            MediaPlayerState result = new MediaPlayerState();
            result.Position = player.Position;
            result.IsPaused = player.CurrentState == MediaElementState.Paused;
            PlaylistPlugin playlistPlugin = player.Plugins.OfType<PlaylistPlugin>().FirstOrDefault();
            if(playlistPlugin != null) {
                result.PlaylistItemIndex = playlistPlugin.CurrentPlaylistItemIndex; }
            return result; }

        public static void RestorePlayerState(this MediaPlayer player, MediaPlayerState state) {
            if(state == null) {
                throw new ArgumentNullException("state"); }
            player.StartupPosition = state.Position;
            player.AutoPlay = !state.IsPaused;
            player.AutoLoad = true;
            PlaylistPlugin playlistPlugin = player.Plugins.OfType<PlaylistPlugin>().FirstOrDefault();
            if(playlistPlugin != null) {
                playlistPlugin.StartupPlaylistItemIndex = state.PlaylistItemIndex; } } }

    public sealed class MediaPlayerState {
        public bool IsPaused {
            get; set; }
        public TimeSpan Position {
            get; set; }
        public int PlaylistItemIndex {
            get; set; } } }
