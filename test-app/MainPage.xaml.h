#pragma once

#include "MainPage.g.h"

namespace test_app
{
    public ref class MainPage sealed
    {
    public:
        MainPage();
    protected:
        virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
    private:
        void Page_Loaded(Platform::Object ^sender, Windows::UI::Xaml::RoutedEventArgs ^e);
        void Player_MediaOpened(Platform::Object ^sender, Windows::UI::Xaml::RoutedEventArgs ^e);
        void seekTimer_Tick(Platform::Object^ sender, Platform::Object^ e);
        void SeekBar_PointerPressed(Platform::Object ^sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs ^e);
        void SeekBar_PointerCaptureLost(Platform::Object ^sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs ^e);
        void SeekBar_ValueChanged(Platform::Object ^sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs ^e);
        void Button_Click_1(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void Button_Click_2(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
    private:
        Windows::Foundation::EventRegistrationToken loadedToken;
        Windows::Media::MediaExtensionManager ^extensions;
        Windows::UI::Xaml::DispatcherTimer ^seekTimer;
        bool seeking;
    };
}
