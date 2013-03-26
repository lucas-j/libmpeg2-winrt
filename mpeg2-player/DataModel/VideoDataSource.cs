using System;
using System.Linq;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Collections.Specialized;
using Windows.Foundation.Metadata;
using Windows.Storage;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Media.Imaging;
using mpeg2_player.Common;

namespace mpeg2_player.Data {
    [WebHostHidden] public abstract class VideoDataCommon: BindableBase {
        public static readonly Uri appx = new Uri("ms-appx:///");
        private int id = 0;
        private string title = string.Empty;

        public VideoDataCommon(int id, String title) {
            this.id = id;
            this.title = title; }

        public int ID {
            get {
                return id; }
            set {
                SetProperty(ref id, value); } }

        public string Title {
            get {
                return title; }
            set {
                SetProperty(ref title, value); } }

        public override string ToString() {
            return Title; } }

    public class VideoDataItem: VideoDataCommon {
        private StorageFile file = null;
        private StorageFolder dir = null;
        private ImageSource img = null;
        private string imgPath = string.Empty;

        public VideoDataItem(int id, String title, String imgPath, StorageFile file): base(id, title) {
            this.file = file;
            this.imgPath = imgPath; }

        public VideoDataItem(int id, String title, String imgPath, StorageFolder dir): base(id, title) {
            this.dir = dir;
            this.imgPath = imgPath; }

        public VideoDataItem(int id, String title, String imgPath): base(id, title) {
            this.imgPath = imgPath; }

        public VideoDataItem(int id, String title): base(id, title) {}

        public StorageFile File {
            get {
                return file; }
            set {
                Dir = null;
                SetProperty(ref file, value); } }

        public StorageFolder Dir {
            get {
                return dir; }
            set {
                File = null;
                SetProperty(ref dir, value); } }

        public string Path {
            get {
                if(file != null) {
                    return file.Path; }
                else if(dir != null) {
                    return dir.Path; }
                else {
                    return string.Empty; } } }

        new public string Title {
            get {
                if(file != null) {
                    return file.Name; }
                else if(dir != null) {
                    return dir.Name; }
                else {
                    return string.Empty; } } }

        public ImageSource Image {
            get {
                if(img == null && imgPath != null) {
                    img = new BitmapImage(new Uri(VideoDataCommon.appx, imgPath)); }
                return img; }

            set {
                imgPath = null;
                SetProperty(ref img, value); } }

        public void SetImage(String path) {
            img = null;
            imgPath = path;
            OnPropertyChanged("Image"); } }

    public class VideoDataGroup: VideoDataCommon
    {
        private ObservableCollection<VideoDataItem> items = new ObservableCollection<VideoDataItem>();
        private ObservableCollection<VideoDataItem> topItems = new ObservableCollection<VideoDataItem>();

        public VideoDataGroup(int id, String title): base(id, title) {
            Items.CollectionChanged += ItemsCollectionChanged; }

        private void ItemsCollectionChanged(object sender, System.Collections.Specialized.NotifyCollectionChangedEventArgs e) {
            // Provides a subset of the full items collection to bind to from a GroupedItemsPage
            // for two reasons: GridView will not virtualize large items collections, and it
            // improves the user experience when browsing through groups with large numbers of
            // items.
            //
            // A maximum of 12 items are displayed because it results in filled grid columns
            // whether there are 1, 2, 3, 4, or 6 rows displayed
            switch(e.Action) {
                case NotifyCollectionChangedAction.Add:
                    if(e.NewStartingIndex < 12) {
                        TopItems.Insert(e.NewStartingIndex,Items[e.NewStartingIndex]);
                        if(TopItems.Count > 12) {
                            TopItems.RemoveAt(12); } }
                    break;
                case NotifyCollectionChangedAction.Move:
                    if(e.OldStartingIndex < 12 && e.NewStartingIndex < 12) {
                        TopItems.Move(e.OldStartingIndex, e.NewStartingIndex); }
                    else if(e.OldStartingIndex < 12) {
                        TopItems.RemoveAt(e.OldStartingIndex);
                        TopItems.Add(Items[11]); }
                    else if(e.NewStartingIndex < 12) {
                        TopItems.Insert(e.NewStartingIndex, Items[e.NewStartingIndex]);
                        TopItems.RemoveAt(12); }
                    break;
                case NotifyCollectionChangedAction.Remove:
                    if(e.OldStartingIndex < 12) {
                        TopItems.RemoveAt(e.OldStartingIndex);
                        if(Items.Count >= 12) {
                            TopItems.Add(Items[11]); } }
                    break;
                case NotifyCollectionChangedAction.Replace:
                    if(e.OldStartingIndex < 12) {
                        TopItems[e.OldStartingIndex] = Items[e.OldStartingIndex]; }
                    break;
                case NotifyCollectionChangedAction.Reset:
                    TopItems.Clear();
                    while(TopItems.Count < Items.Count && TopItems.Count < 12) {
                        TopItems.Add(Items[TopItems.Count]); }
                    break; } }

        public ObservableCollection<VideoDataItem> Items {
            get {
                return items; } }

        public ObservableCollection<VideoDataItem> TopItems {
            get {
                return topItems; } } }

    public sealed class VideoDataSource {
        private static VideoDataSource singleton = new VideoDataSource();
        private ObservableCollection<VideoDataGroup> groups = new ObservableCollection<VideoDataGroup>();
        public VideoDataSource() {
            VideoDataGroup recent = new VideoDataGroup(0, "Recent");
            VideoDataGroup library = new VideoDataGroup(1, "Video Library");
            Groups.Add(recent);
            Groups.Add(library); }

        public static IEnumerable<VideoDataGroup> GetGroups() {
            return singleton.Groups; }

        public static VideoDataGroup GetGroup(int id) {
            IEnumerable<VideoDataGroup> matches = singleton.Groups.Where((group) => group.ID.Equals(id));
            if(matches.Count() == 1) {
                return matches.First(); }
            return null; }

        public static VideoDataItem GetItem(int id) {
            IEnumerable<VideoDataItem> matches = singleton.Groups.SelectMany(group => group.Items).Where((item) => item.ID.Equals(id));
            if(matches.Count() == 1) {
                return matches.First(); }
            return null; }

        public static int NextID() {
            int maxID = -1;
            foreach(VideoDataGroup group in GetGroups()) {
                if(group.Items.Count() == 0) {
                    if(maxID == -1) {
                        maxID = 0; } }
                else {
                    int id = group.Items.Max(item => item.ID);
                    if(id > maxID) {
                        maxID = id; } } }
            return maxID + 1; }

        private static int NextGroupID() {
            return GetGroups().Max(group => group.ID) + 1; }

        public static int AddGroup(String title) {
            int id = NextGroupID();
            singleton.Groups.Add(new VideoDataGroup(id, title));
            return id; }

        public ObservableCollection<VideoDataGroup> Groups {
            get {
                return groups; } } } }
