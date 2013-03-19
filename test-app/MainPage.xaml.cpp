#include "pch.h"
#include "MainPage.xaml.h"

#define INITGUID
#include <guiddef.h>
#include <cguid.h>
#include <mfapi.h>
#include <comdef.h>
#include <time.h>

using namespace test_app;

using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::Media;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Concurrency;

MainPage::MainPage()
{
    InitializeComponent();
    extensions = ref new MediaExtensionManager();
    loadedToken = Loaded += ref new RoutedEventHandler(this, &MainPage::Page_Loaded);
}

void MainPage::Page_Loaded(Object ^sender, RoutedEventArgs ^e)
{
    extensions->RegisterVideoDecoder("libmpeg2.Decoder", Guid(MFVideoFormat_MPEG2), Guid(GUID_NULL));
    extensions->RegisterVideoDecoder("libmpeg2.Decoder", Guid(MFVideoFormat_MPG1), Guid(GUID_NULL));
}

void MainPage::OnNavigatedTo(NavigationEventArgs^ e) {}

void MainPage::Button_Click_1(Object^ sender, RoutedEventArgs^ e) {
    auto picker = ref new Pickers::FileOpenPicker();
    auto player = Player;
    picker->SuggestedStartLocation = Pickers::PickerLocationId::Desktop;
    picker->FileTypeFilter->Append(".mpg");
    picker->FileTypeFilter->Append(".m2v");
    picker->FileTypeFilter->Append(".ts");
    picker->FileTypeFilter->Append(".wtv");
    picker->FileTypeFilter->Append(".wmv");
    picker->FileTypeFilter->Append(".m4v");
    task<StorageFile ^>(picker->PickSingleFileAsync()).then([player](StorageFile ^file) {
        if(file) {
            auto content = file->ContentType;
            task<IRandomAccessStream ^>(file->OpenAsync(FileAccessMode::Read)).then([player, content](IRandomAccessStream ^stream) {
                player->Stop();
                player->SetSource(stream, content);
            });
        }
    });
}

void MainPage::Button_Click_2(Object^ sender, RoutedEventArgs^ e) {
    Player->Stop();
    Player->Source = nullptr; }
