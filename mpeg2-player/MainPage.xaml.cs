using System;
using System.Collections.Generic;
using Windows.UI;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using mpeg2_player.Common;
using mpeg2_player.Data;

namespace mpeg2_player {
    public sealed partial class MainPage: LayoutAwarePage {
        public MainPage() {
            this.InitializeComponent(); }

        protected override void LoadState(Object param, Dictionary<String, Object> state) {
            DefaultViewModel["Groups"] = VideoDataSource.GetGroups(); }

        private void ItemView_ItemClick(object sender, ItemClickEventArgs e) {
            VideoDataItem item = e.ClickedItem as VideoDataItem;
            if(item.ID >= 0) {
                Frame.Navigate(typeof(PlayerPage), item.ID); } }

        private async void OpenFileButton_Click(object sender, RoutedEventArgs e) {

            
            int id = await App.Current.PickFile();
            if(id >= 0) {
                Frame.Navigate(typeof(PlayerPage), id); 
            } 
        }

        private void Header_Click(object sender, RoutedEventArgs e) {
            FrameworkElement elem = sender as FrameworkElement;
            VideoDataGroup group = elem.DataContext as VideoDataGroup;
            Frame.Navigate(typeof(DirectoryPage), group.ID); } } }
