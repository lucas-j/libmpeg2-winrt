using System;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using Windows.Foundation.Metadata;
using Windows.UI.Xaml.Data;

namespace mpeg2_player.Common {
    [WebHostHidden] public abstract class BindableBase: INotifyPropertyChanged {
        public event PropertyChangedEventHandler PropertyChanged;

        protected bool SetProperty<T>(ref T storage, T value, [CallerMemberName] String propertyName = null) {
            if(object.Equals(storage, value)) {
                return false; }
            storage = value;
            this.OnPropertyChanged(propertyName);
            return true; }

        protected void OnPropertyChanged([CallerMemberName] string propertyName = null) {
            PropertyChangedEventHandler eventHandler = this.PropertyChanged;
            if(eventHandler != null) {
                eventHandler(this, new PropertyChangedEventArgs(propertyName)); } } } }
