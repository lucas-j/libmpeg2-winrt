#include "pch.h"
#include "MainPage.xaml.h"

#define INITGUID
#include <guiddef.h>
#include <cguid.h>
#include <mfapi.h>
#include <comdef.h>
#include <time.h>
#include <iostream>
using std::cout;
using std::endl;

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
    seeking = false;
}

void MainPage::Page_Loaded(Object ^sender, RoutedEventArgs ^e)
{
    extensions->RegisterVideoDecoder("libmpeg2.Decoder", Guid(MFVideoFormat_MPEG2), Guid(GUID_NULL));
    extensions->RegisterVideoDecoder("libmpeg2.Decoder", Guid(MFVideoFormat_MPG1), Guid(GUID_NULL));
    Player->MediaOpened += ref new RoutedEventHandler(this, &MainPage::Player_MediaOpened);
    seekTimer = ref new DispatcherTimer();
    TimeSpan ts;
    ts.Duration = 10 * 1000;
    seekTimer->Interval = ts;
    seekTimer->Tick += ref new EventHandler<Object ^>(this, &MainPage::seekTimer_Tick);
    SeekBar->PointerPressed += ref new PointerEventHandler(this, &MainPage::SeekBar_PointerPressed);
    SeekBar->PointerCaptureLost += ref new PointerEventHandler(this, &MainPage::SeekBar_PointerCaptureLost);
    SeekBar->ValueChanged += ref new RangeBaseValueChangedEventHandler(this, &MainPage::SeekBar_ValueChanged);
}

void MainPage::Player_MediaOpened(Object ^sender, RoutedEventArgs ^e)
{
    if(Player->NaturalDuration.HasTimeSpan)
    {
        TimeSpan ts = Player->NaturalDuration.TimeSpan;
        double ms = ts.Duration / 10.0 / 1000;
        SeekBar->Maximum = ms;
        SeekBar->SmallChange = 100;
        SeekBar->LargeChange = min(1000, ms / 20);
        seekTimer->Start();
    }
}

void MainPage::seekTimer_Tick(Object ^sender, Object ^e)
{
    if(!seeking) {
        SeekBar->Value = Player->Position.Duration / 10.0 / 1000; }
}

void MainPage::SeekBar_PointerPressed(Object ^sender, PointerRoutedEventArgs ^e)
{
    seeking = true;
}

void MainPage::SeekBar_PointerCaptureLost(Object ^sender, PointerRoutedEventArgs ^e)
{
    seeking = false;
}

void MainPage::SeekBar_ValueChanged(Object ^sender, RangeBaseValueChangedEventArgs ^e)
{
    if(!seeking)
    {
        TimeSpan ts;
        ts.Duration = static_cast<long long>(SeekBar->Value * 10 * 1000);
        Player->Position = ts;
    }
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
    if(Player->CurrentState == MediaElementState::Paused) {
        Player->Play(); }
    else {
        Player->Pause(); }
    /*Player->Stop();
    Player->Source = nullptr;*/ }
