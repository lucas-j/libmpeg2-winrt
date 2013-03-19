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
        void Button_Click_1(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        Windows::Foundation::EventRegistrationToken loadedToken;
        Windows::Media::MediaExtensionManager ^extensions;
        void Button_Click_2(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
    };
}
