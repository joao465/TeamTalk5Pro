from pathlib import Path

def replace_once(path, old, new):
    p = Path(path)
    data = p.read_bytes()
    eol = "\r\n" if b"\r\n" in data[:20000] else "\n"
    oldb = old.replace("\n", eol).encode("utf-8")
    newb = new.replace("\n", eol).encode("utf-8")
    count = data.count(oldb)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}")
    p.write_bytes(data.replace(oldb, newb, 1))

def insert_before(path, marker, text):
    p = Path(path)
    data = p.read_bytes()
    eol = "\r\n" if b"\r\n" in data[:20000] else "\n"
    markerb = marker.replace("\n", eol).encode("utf-8")
    textb = text.replace("\n", eol).encode("utf-8")
    count = data.count(markerb)
    if count != 1:
        raise SystemExit(f"{path}: expected one marker, found {count}")
    p.write_bytes(data.replace(markerb, textb + markerb, 1))

replace_once(
    "Client/qtTeamTalk/settings.h",
    '''#define SETTINGS_SOUND_INPUTDEVICE                  "soundsystem/inputdeviceid"
#define SETTINGS_SOUND_INPUTDEVICE_DEFAULT          SOUNDDEVICEID_DEFAULT
#define SETTINGS_SOUND_INPUTDEVICE_UID              "soundsystem/inputdeviceuid"
#define SETTINGS_SOUND_OUTPUTDEVICE                 "soundsystem/outputdeviceid"''',
    '''#define SETTINGS_SOUND_INPUTDEVICE                  "soundsystem/inputdeviceid"
#define SETTINGS_SOUND_INPUTDEVICE_DEFAULT          SOUNDDEVICEID_DEFAULT
#define SETTINGS_SOUND_INPUTDEVICE_UID              "soundsystem/inputdeviceuid"
#define SETTINGS_SOUND_SECONDARY_INPUTDEVICE        "soundsystem/secondary-inputdeviceid"
#define SETTINGS_SOUND_SECONDARY_INPUTDEVICE_DEFAULT TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL
#define SETTINGS_SOUND_SECONDARY_INPUTDEVICE_UID    "soundsystem/secondary-inputdeviceuid"
#define SETTINGS_SOUND_OUTPUTDEVICE                 "soundsystem/outputdeviceid"'''
)

replace_once(
    "Client/qtTeamTalk/utilsound.h",
    '''int getSelectedSndInputDevice();
int getSelectedSndOutputDevice();''',
    '''int getSelectedSndInputDevice();
int getSelectedSecondarySndInputDevice();
int getSelectedSndOutputDevice();'''
)

replace_once(
    "Client/qtTeamTalk/utilsound.cpp",
    '''int getSelectedSndOutputDevice()
{''',
    '''int getSelectedSecondarySndInputDevice()
{
    int inputid = ttSettings->value(SETTINGS_SOUND_SECONDARY_INPUTDEVICE,
                          SETTINGS_SOUND_SECONDARY_INPUTDEVICE_DEFAULT).toInt();
    if (inputid == TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL)
        return inputid;

    qDebug() << "Secondary input device in settings #" << inputid;
    if (inputid == SOUNDDEVICEID_DEFAULT)
        inputid = getDefaultSndInputDevice();
    else
    {
        QString uid = ttSettings->value(SETTINGS_SOUND_SECONDARY_INPUTDEVICE_UID, "").toString();
        if (uid.size())
  inputid = getSoundInputFromUID(inputid, uid);
    }
    qDebug() << "Returning secondary input device #" << inputid;
    return inputid;
}

int getSelectedSndOutputDevice()
{'''
)
replace_once(
    "Client/qtTeamTalk/utilsound.cpp",
    '''    TT_CloseSoundInputDevice(ttInst);
    TT_CloseSoundOutputDevice(ttInst);
    TT_CloseSoundDuplexDevices(ttInst);

    //Restart sound system so we have the latest sound devices''',
    '''    TT_CloseSecondarySoundInputDevice(ttInst);
    TT_CloseSoundInputDevice(ttInst);
    TT_CloseSoundOutputDevice(ttInst);
    TT_CloseSoundDuplexDevices(ttInst);

    //Restart sound system so we have the latest sound devices'''
)
replace_once(
    "Client/qtTeamTalk/utilsound.cpp",
    '''QStringList initSelectedSoundDevices(SoundDevice& indev, SoundDevice& outdev)
{
    int inputid = getSelectedSndInputDevice();
    int outputid = getSelectedSndOutputDevice();

    QVector<SoundDevice> devs = getSoundDevices();
    getSoundDevice(inputid, devs, indev);
    getSoundDevice(outputid, devs, outdev);

    return initSoundDevices(indev, outdev);
}''',
    '''QStringList initSelectedSoundDevices(SoundDevice& indev, SoundDevice& outdev)
{
    int inputid = getSelectedSndInputDevice();
    int secondaryinputid = getSelectedSecondarySndInputDevice();
    int outputid = getSelectedSndOutputDevice();

    QVector<SoundDevice> devs = getSoundDevices();
    getSoundDevice(inputid, devs, indev);
    getSoundDevice(outputid, devs, outdev);

    QStringList result = initSoundDevices(indev, outdev);

    if (secondaryinputid != TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL)
    {
        SoundDevice secondarydev = {};
        if (!getSoundDevice(secondaryinputid, devs, secondarydev))
        {
  result.append(QObject::tr("Failed to find secondary sound input device"));
        }
        else if (secondarydev.nDeviceID == indev.nDeviceID)
        {
  result.append(QObject::tr("Secondary sound input device must be different from the primary input device"));
        }
        else if (!TT_InitSecondarySoundInputDevice(ttInst, secondarydev.nDeviceID))
        {
  result.append(QObject::tr("Failed to initialize secondary sound input device: %1")
                    .arg(_Q(secondarydev.szDeviceName)));
        }
    }

    return result;
}'''
)
replace_once(
    "Client/qtTeamTalk/utilsound.cpp",
    '''QStringList initDefaultSoundDevices(SoundDevice& indev, SoundDevice& outdev)
{
    QStringList result;

    TT_CloseSoundInputDevice(ttInst);''',
    '''QStringList initDefaultSoundDevices(SoundDevice& indev, SoundDevice& outdev)
{
    QStringList result;

    TT_CloseSecondarySoundInputDevice(ttInst);
    TT_CloseSoundInputDevice(ttInst);'''
)

replace_once(
    "Library/TeamTalk_DLL/TeamTalk.h",
    '''    TEAMTALKDLL_API TTBOOL TT_InitSoundInputDevice(IN TTInstance* lpTTInstance, 
                                         IN INT32 nInputDeviceID);

    /**''',
    '''    TEAMTALKDLL_API TTBOOL TT_InitSoundInputDevice(IN TTInstance* lpTTInstance, 
                                         IN INT32 nInputDeviceID);

    /**
     * @brief Initialize an optional secondary sound input device.
     *
     * Audio from this device is mixed with the primary microphone before
     * voice encoding. The TeamTalk Pro microphone equalizer is applied only
     * to the primary input and never to this secondary input.
     */
    TEAMTALKDLL_API TTBOOL TT_InitSecondarySoundInputDevice(IN TTInstance* lpTTInstance,
                                                  IN INT32 nInputDeviceID);

    /** @brief Close the optional secondary sound input device. */
    TEAMTALKDLL_API TTBOOL TT_CloseSecondarySoundInputDevice(IN TTInstance* lpTTInstance);

    /**'''
)

insert_before(
    "Library/TeamTalkLib/bin/dll/TeamTalk.cpp",
    '''TEAMTALKDLL_API TTBOOL TT_CloseSoundInputDevice(IN TTInstance* lpTTInstance)''',
    '''TEAMTALKDLL_API TTBOOL TT_InitSecondarySoundInputDevice(IN TTInstance* lpTTInstance,
                                                  IN INT32 nInputDeviceID)
{
    clientnode_t clientnode;
    GET_CLIENTNODE_RET(clientnode, lpTTInstance, FALSE);
    return static_cast<TTBOOL>(clientnode->InitSecondarySoundInputDevice(nInputDeviceID));
}

TEAMTALKDLL_API TTBOOL TT_CloseSecondarySoundInputDevice(IN TTInstance* lpTTInstance)
{
    clientnode_t clientnode;
    GET_CLIENTNODE_RET(clientnode, lpTTInstance, FALSE);
    return static_cast<TTBOOL>(clientnode->CloseSecondarySoundInputDevice());
}

'''
)

replace_once(
    "Library/TeamTalkLib/teamtalk/client/ClientNode.h",
    '''#include <atomic>
#include <cstdint>
#include <map>
#include <memory>''',
    '''#include <atomic>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>'''
)
replace_once(
    "Library/TeamTalkLib/teamtalk/client/ClientNode.h",
    '''    public:
        ClientNode(const ACE_TString& version, ClientListener* listener);''',
    '''    public:
        struct SecondarySoundCapture;

        ClientNode(const ACE_TString& version, ClientListener* listener);'''
)
replace_once(
    "Library/TeamTalkLib/teamtalk/client/ClientNode.h",
    '''        bool InitSoundInputDevice(int inputdeviceid);
        bool InitSoundOutputDevice(int outputdeviceid);''',
    '''        bool InitSoundInputDevice(int inputdeviceid);
        bool InitSecondarySoundInputDevice(int inputdeviceid);
        bool CloseSecondarySoundInputDevice();
        bool InitSoundOutputDevice(int outputdeviceid);'''
)
replace_once(
    "Library/TeamTalkLib/teamtalk/client/ClientNode.h",
    '''        void OpenAudioCapture(const AudioCodec& codec);
        void CloseAudioCapture();
        void QueueAudioCapture(media::AudioFrame& audframe);''',
    '''        void OpenAudioCapture(const AudioCodec& codec);
        void CloseAudioCapture();
        bool OpenSecondaryAudioCapture(const AudioCodec& codec);
        void CloseSecondaryAudioCapture();
        void StreamSecondaryCaptureCb(const soundsystem::InputStreamer& streamer,
                            const short* buffer, int n_samples);
        void MixSecondaryAudio(media::AudioFrame& audframe);
        void QueueAudioCapture(media::AudioFrame& audframe);'''
)
replace_once(
    "Library/TeamTalkLib/teamtalk/client/ClientNode.h",
    '''        SoundProperties m_soundprop;
        std::unique_ptr<SoundInputEqualizerState> m_soundinput_equalizer;
        //log voice to files''',
    '''        SoundProperties m_soundprop;
        std::unique_ptr<SoundInputEqualizerState> m_soundinput_equalizer;
        std::unique_ptr<SecondarySoundCapture> m_secondary_capture;
        int m_secondary_inputdeviceid = SOUNDDEVICE_IGNORE_ID;
        bool m_secondary_capture_open = false;
        std::deque<short> m_secondary_mix_queue;
        std::mutex m_secondary_mix_mutex;
        //log voice to files'''
)
replace_once(
    "Library/TeamTalkLib/teamtalk/client/ClientNode.h",
    '''        audio_resampler_t m_capture_resampler;
        std::vector<short> m_capture_buffer;
        //audio resampler for playback''',
    '''        audio_resampler_t m_capture_resampler;
        std::vector<short> m_capture_buffer;
        audio_resampler_t m_secondary_capture_resampler;
        std::vector<short> m_secondary_capture_buffer;
        //audio resampler for playback'''
)

insert_before(
    "Library/TeamTalkLib/teamtalk/client/ClientNode.cpp",
    '''ClientNode::ClientNode(const ACE_TString& version, ClientListener* listener)''',
    '''struct ClientNode::SecondarySoundCapture : public soundsystem::StreamCapture
{
    explicit SecondarySoundCapture(ClientNode* owner)
        : owner(owner)
    {
    }

    void StreamCaptureCb(const soundsystem::InputStreamer& streamer,
               const short* buffer, int samples) override
    {
        owner->StreamSecondaryCaptureCb(streamer, buffer, samples);
    }

    soundsystem::SoundDeviceFeatures GetCaptureFeatures() override
    {
        return soundsystem::SOUNDDEVICEFEATURE_NONE;
    }

    ClientNode* owner;
};

'''
)
replace_once(
    "Library/TeamTalkLib/teamtalk/client/ClientNode.cpp",
    '''    m_soundsystem = soundsystem::GetInstance();

    m_soundprop.soundgroupid = m_soundsystem->OpenSoundGroup();''',
    '''    m_soundsystem = soundsystem::GetInstance();
    m_secondary_capture = std::make_unique<SecondarySoundCapture>(this);

    m_soundprop.soundgroupid = m_soundsystem->OpenSoundGroup();'''
)
replace_once(
    "Library/TeamTalkLib/teamtalk/client/ClientNode.cpp",
    '''        CloseVideoCapture();
        CloseSoundInputDevice();''',
    '''        CloseVideoCapture();
        CloseSecondarySoundInputDevice();
        CloseSoundInputDevice();'''
)

insert_before(
    "Library/TeamTalkLib/teamtalk/client/ClientNode.cpp",
    '''bool ClientNode::InitSoundInputDevice(int inputdevice)''',
    '''bool ClientNode::InitSecondarySoundInputDevice(int inputdevice)
{
    ASSERT_CLIENTNODE_LOCKED(this);

    if (inputdevice == SOUNDDEVICE_IGNORE_ID ||
        inputdevice == m_soundprop.inputdeviceid ||
        !m_soundsystem->CheckInputDevice(inputdevice))
        return false;

    CloseSecondaryAudioCapture();
    m_secondary_inputdeviceid = inputdevice;

    if (m_mychannel && !OpenSecondaryAudioCapture(m_mychannel->GetAudioCodec()))
    {
        m_secondary_inputdeviceid = SOUNDDEVICE_IGNORE_ID;
        return false;
    }

    return true;
}

bool ClientNode::CloseSecondarySoundInputDevice()
{
    ASSERT_CLIENTNODE_LOCKED(this);

    CloseSecondaryAudioCapture();
    m_secondary_inputdeviceid = SOUNDDEVICE_IGNORE_ID;
    return true;
}

'''
)

replace_once(
    "Library/TeamTalkLib/teamtalk/client/ClientNode.cpp",
    '''    if (!opened)
    {
        if (m_listener != nullptr)
  m_listener->OnInternalError(TT_INTERR_SNDINPUT_FAILURE,
                              GetErrorDescription(TT_INTERR_SNDINPUT_FAILURE));
    }
}

void ClientNode::CloseAudioCapture()''',
    '''    if (!opened)
    {
        if (m_listener != nullptr)
  m_listener->OnInternalError(TT_INTERR_SNDINPUT_FAILURE,
                              GetErrorDescription(TT_INTERR_SNDINPUT_FAILURE));
    }
    else if (m_secondary_inputdeviceid != SOUNDDEVICE_IGNORE_ID)
    {
        OpenSecondaryAudioCapture(codec);
    }
}

void ClientNode::CloseAudioCapture()'''
)
replace_once(
    "Library/TeamTalkLib/teamtalk/client/ClientNode.cpp",
    '''void ClientNode::CloseAudioCapture()
{
    ASSERT_CLIENTNODE_LOCKED(this);

    if((m_flags & CLIENT_SNDINOUTPUT_DUPLEX) != 0u)''',
    '''void ClientNode::CloseAudioCapture()
{
    ASSERT_CLIENTNODE_LOCKED(this);

    CloseSecondaryAudioCapture();

    if((m_flags & CLIENT_SNDINOUTPUT_DUPLEX) != 0u)'''
)
replace_once(
    "Library/TeamTalkLib/teamtalk/client/ClientNode.cpp",
    '''void ClientNode::QueueAudioCapture(media::AudioFrame& audframe)
{
    if (m_soundinput_equalizer)
        m_soundinput_equalizer->Process(audframe);

    bool const ptt_close''',
    '''void ClientNode::QueueAudioCapture(media::AudioFrame& audframe)
{
    if (m_soundinput_equalizer)
        m_soundinput_equalizer->Process(audframe);

    // Mix after the Pro EQ so the secondary microphone/line input
    // always bypasses the microphone equalizer.
    MixSecondaryAudio(audframe);

    bool const ptt_close'''
)

insert_before(
    "Library/TeamTalkLib/teamtalk/client/ClientNode.cpp",
    '''// Separate thread
void ClientNode::StreamCaptureCb(const soundsystem::InputStreamer& /*streamer*/,''',
    '''bool ClientNode::OpenSecondaryAudioCapture(const AudioCodec& codec)
{
    ASSERT_CLIENTNODE_LOCKED(this);

    if (!m_secondary_capture ||
        m_secondary_inputdeviceid == SOUNDDEVICE_IGNORE_ID)
        return true;

    if (m_secondary_capture_open)
        CloseSecondaryAudioCapture();

    int const codec_samplerate = GetAudioCodecSampleRate(codec);
    int const codec_samples = GetAudioCodecCbSamples(codec);
    int const codec_channels = GetAudioCodecChannels(codec);

    if (codec_samples <= 0 || codec_samplerate <= 0 || codec_channels == 0)
        return false;

    int input_samplerate = codec_samplerate;
    int input_channels = codec_channels;
    int input_samples = codec_samples;

    if (!m_soundsystem->SupportsInputFormat(m_secondary_inputdeviceid,
                                  codec_channels, codec_samplerate))
    {
        DeviceInfo dev;
        if (!m_soundsystem->GetDevice(m_secondary_inputdeviceid, dev) ||
  dev.default_samplerate == 0)
  return false;

        input_samplerate = dev.default_samplerate;
        input_channels = dev.GetSupportedInputChannels(codec_channels);
        input_samples = CalcSamples(codec_samplerate, codec_samples,
                          input_samplerate);

        media::AudioFormat infmt(input_samplerate, input_channels);
        media::AudioFormat outfmt(codec_samplerate, codec_channels);
        m_secondary_capture_resampler = MakeAudioResampler(infmt, outfmt);
        if (!m_secondary_capture_resampler)
  return false;

        m_secondary_capture_buffer.resize(size_t(codec_samples) * codec_channels);
    }
    else
    {
        m_secondary_capture_resampler.reset();
        m_secondary_capture_buffer.clear();
    }

    m_secondary_capture_open = m_soundsystem->OpenInputStream(
        m_secondary_capture.get(), m_secondary_inputdeviceid,
        m_soundprop.soundgroupid, input_samplerate, input_channels,
        input_samples);

    return m_secondary_capture_open;
}

void ClientNode::CloseSecondaryAudioCapture()
{
    if (m_secondary_capture_open && m_secondary_capture)
        m_soundsystem->CloseInputStream(m_secondary_capture.get());

    m_secondary_capture_open = false;
    m_secondary_capture_resampler.reset();
    m_secondary_capture_buffer.clear();

    std::lock_guard<std::mutex> const g(m_secondary_mix_mutex);
    m_secondary_mix_queue.clear();
}

void ClientNode::StreamSecondaryCaptureCb(const soundsystem::InputStreamer& /*streamer*/,
                                const short* buffer, int n_samples)
{
    rguard_t const g_snd(LockSndprop());

    if (!m_secondary_capture_open)
        return;

    int const codec_samplerate = GetAudioCodecSampleRate(m_voice_thread.Codec());
    int const codec_samples = GetAudioCodecCbSamples(m_voice_thread.Codec());
    int const codec_channels = GetAudioCodecChannels(m_voice_thread.Codec());

    if (codec_samples <= 0 || codec_samplerate <= 0 || codec_channels == 0)
        return;

    const short* capture_buffer = buffer;
    if (m_secondary_capture_resampler)
    {
        if ((int)m_secondary_capture_buffer.size() != codec_samples * codec_channels)
  return;

        int const ret = m_secondary_capture_resampler->Resample(
  buffer, n_samples, m_secondary_capture_buffer.data(),
  codec_samples);
        if (ret <= 0)
  return;

        capture_buffer = m_secondary_capture_buffer.data();
    }

    size_t const sample_count = size_t(codec_samples) * codec_channels;
    std::lock_guard<std::mutex> const g(m_secondary_mix_mutex);
    for (size_t i = 0; i < sample_count; ++i)
        m_secondary_mix_queue.push_back(capture_buffer[i]);

    while (m_secondary_mix_queue.size() > sample_count * 4)
    {
        for (size_t i = 0; i < sample_count; ++i)
  m_secondary_mix_queue.pop_front();
    }
}

void ClientNode::MixSecondaryAudio(media::AudioFrame& audframe)
{
    if (!audframe.input_buffer || audframe.input_samples <= 0 ||
        audframe.inputfmt.channels <= 0)
        return;

    size_t const sample_count =
        size_t(audframe.input_samples) * audframe.inputfmt.channels;

    std::lock_guard<std::mutex> const g(m_secondary_mix_mutex);
    if (m_secondary_mix_queue.size() < sample_count)
        return;

    while (m_secondary_mix_queue.size() >= sample_count * 2)
    {
        for (size_t i = 0; i < sample_count; ++i)
  m_secondary_mix_queue.pop_front();
    }

    short* dst = const_cast<short*>(audframe.input_buffer);
    for (size_t i = 0; i < sample_count; ++i)
    {
        int const mixed = int(dst[i]) + int(m_secondary_mix_queue.front());
        m_secondary_mix_queue.pop_front();
        dst[i] = short(std::clamp(mixed, -32768, 32767));
    }
}

// Separate thread
'''
)

replace_once(
    "Client/qtTeamTalk/preferences.ui",
    '''               <string>Input device</string>''',
    '''               <string>Microfone principal</string>'''
)
replace_once(
    "Client/qtTeamTalk/preferences.ui",
    '''            <item row="2" column="0">
   <widget class="QLabel" name="label_14">''',
    '''            <item row="2" column="0">
   <widget class="QLabel" name="secondaryInputLabel">
    <property name="text">
     <string>Microfone secundário</string>
    </property>
    <property name="buddy">
     <cstring>secondaryInputDevBox</cstring>
    </property>
   </widget>
  </item>
  <item row="2" column="1">
   <widget class="QComboBox" name="secondaryInputDevBox"/>
  </item>
  <item row="2" column="2">
   <widget class="QPushButton" name="secondaryMicListenButton">
    <property name="text">
     <string>Escuta do mic secundário</string>
    </property>
    <property name="checkable">
     <bool>true</bool>
    </property>
   </widget>
  </item>
  <item row="3" column="0">
   <widget class="QLabel" name="label_14">'''
)
replace_once(
    "Client/qtTeamTalk/preferences.ui",
    '''            <item row="2" column="1">
   <widget class="QComboBox" name="outputdevBox"/>''',
    '''            <item row="3" column="1">
   <widget class="QComboBox" name="outputdevBox"/>'''
)
replace_once(
    "Client/qtTeamTalk/preferences.ui",
    '''            <item row="2" column="2">
   <widget class="QToolButton" name="refreshoutputButton">''',
    '''            <item row="3" column="2">
   <widget class="QToolButton" name="refreshoutputButton">'''
)
replace_once(
    "Client/qtTeamTalk/preferences.ui",
    '''            <item row="3" column="1">
   <widget class="QLabel" name="outputinfoLabel">''',
    '''            <item row="4" column="1">
   <widget class="QLabel" name="outputinfoLabel">'''
)

replace_once(
    "Client/qtTeamTalk/preferencesdlg.h",
    '''    void slotSoundTestDevices(bool checked);
    void slotSoundDefaults();''',
    '''    void slotSoundTestDevices(bool checked);
    void slotSecondaryMicListen(bool checked);
    void restoreSecondarySoundInput();
    void slotSoundDefaults();'''
)
replace_once(
    "Client/qtTeamTalk/preferencesdlg.h",
    '''    TTSoundLoop* m_sndloop;''',
    '''    TTSoundLoop* m_sndloop;
    TTSoundLoop* m_secondaryloop;'''
)
replace_once(
    "Client/qtTeamTalk/preferencesdlg.cpp",
    ''', m_uservideo(nullptr)
, m_sndloop(nullptr)
{''',
    ''', m_uservideo(nullptr)
, m_sndloop(nullptr)
, m_secondaryloop(nullptr)
{'''
)
replace_once(
    "Client/qtTeamTalk/preferencesdlg.cpp",
    '''    connect(ui.inputdevBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
  this, &PreferencesDlg::slotSoundInputChange);
    connect(ui.outputdevBox, QOverload<int>::of(&QComboBox::currentIndexChanged),''',
    '''    connect(ui.inputdevBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
  this, &PreferencesDlg::slotSoundInputChange);
    connect(ui.secondaryMicListenButton, &QAbstractButton::clicked,
  this, &PreferencesDlg::slotSecondaryMicListen);
    connect(ui.outputdevBox, QOverload<int>::of(&QComboBox::currentIndexChanged),'''
)
replace_once(
    "Client/qtTeamTalk/preferencesdlg.cpp",
    '''PreferencesDlg::~PreferencesDlg()
{
    ttSettings->setValue(SETTINGS_DISPLAY_PREFERENCESWINDOWPOS, saveGeometry());
    TT_CloseSoundLoopbackTest(m_sndloop);
}''',
    '''PreferencesDlg::~PreferencesDlg()
{
    ttSettings->setValue(SETTINGS_DISPLAY_PREFERENCESWINDOWPOS, saveGeometry());
    TT_CloseSoundLoopbackTest(m_sndloop);
    if (m_secondaryloop)
    {
        TT_CloseSoundLoopbackTest(m_secondaryloop);
        m_secondaryloop = nullptr;
        restoreSecondarySoundInput();
    }
}'''
)
replace_once(
    "Client/qtTeamTalk/preferencesdlg.cpp",
    '''    ui.inputdevBox->clear();
    ui.outputdevBox->clear();''',
    '''    ui.inputdevBox->clear();
    ui.secondaryInputDevBox->clear();
    ui.secondaryInputDevBox->addItem(tr("Desativado"), TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL);
    ui.outputdevBox->clear();'''
)
replace_once(
    "Client/qtTeamTalk/preferencesdlg.cpp",
    '''        ui.inputdevBox->addItem(_Q(m_sounddevices[i].szDeviceName),
                      m_sounddevices[i].nDeviceID);
    }''',
    '''        ui.inputdevBox->addItem(_Q(m_sounddevices[i].szDeviceName),
                      m_sounddevices[i].nDeviceID);
        ui.secondaryInputDevBox->addItem(_Q(m_sounddevices[i].szDeviceName),
                               m_sounddevices[i].nDeviceID);
    }'''
)
replace_once(
    "Client/qtTeamTalk/preferencesdlg.cpp",
    '''    int index = ui.inputdevBox->findData(devid);
    if(index >= 0)
        ui.inputdevBox->setCurrentIndex(index);

    //for WASAPI, make a default device in the same way as DirectSound''',
    '''    int index = ui.inputdevBox->findData(devid);
    if(index >= 0)
        ui.inputdevBox->setCurrentIndex(index);

    devid = ttSettings->value(SETTINGS_SOUND_SECONDARY_INPUTDEVICE,
                    SETTINGS_SOUND_SECONDARY_INPUTDEVICE_DEFAULT).toInt();
    uid = ttSettings->value(SETTINGS_SOUND_SECONDARY_INPUTDEVICE_UID, "").toString();
    if (getSoundDevice(uid, true, m_sounddevices, dev) && dev.nDeviceID != devid)
        devid = dev.nDeviceID;

    index = ui.secondaryInputDevBox->findData(devid);
    if (index < 0)
        index = ui.secondaryInputDevBox->findData(TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL);
    if (index >= 0)
        ui.secondaryInputDevBox->setCurrentIndex(index);

    //for WASAPI, make a default device in the same way as DirectSound'''
)
replace_once(
    "Client/qtTeamTalk/preferencesdlg.cpp",
    '''    if(flags & CLIENT_SNDINOUTPUT_DUPLEX)
        TT_CloseSoundDuplexDevices(ttInst);''',
    '''    TT_CloseSecondarySoundInputDevice(ttInst);
    if(flags & CLIENT_SNDINOUTPUT_DUPLEX)
        TT_CloseSoundDuplexDevices(ttInst);'''
)

insert_before(
    "Client/qtTeamTalk/preferencesdlg.cpp",
    '''void PreferencesDlg::slotSoundDefaults()''',
    '''void PreferencesDlg::restoreSecondarySoundInput()
{
    int secondaryid = getSelectedSecondarySndInputDevice();
    int primaryid = getSelectedSndInputDevice();
    if (secondaryid != TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL &&
        secondaryid != primaryid)
    {
        TT_InitSecondarySoundInputDevice(ttInst, secondaryid);
    }
}

void PreferencesDlg::slotSecondaryMicListen(bool checked)
{
    if (checked)
    {
        if (ui.sndtestButton->isChecked())
  ui.sndtestButton->click();

        int inputid = ui.secondaryInputDevBox->currentData().toInt();
        if (inputid == TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL)
        {
  QMessageBox::information(this, tr("Escuta do mic secundário"),
                           tr("Selecione um microfone secundário primeiro."));
  ui.secondaryMicListenButton->setChecked(false);
  return;
        }

        SoundSystem sndsys = getSoundSystem();
        int outputid = ui.outputdevBox->currentData().toInt();
        if (outputid == SOUNDDEVICEID_DEFAULT)
  TT_GetDefaultSoundDevicesEx(sndsys, nullptr, &outputid);

        SoundDevice in_dev = {}, out_dev = {};
        if (!getSoundDevice(inputid, m_sounddevices, in_dev) ||
  !getSoundDevice(outputid, m_sounddevices, out_dev))
        {
  ui.secondaryMicListenButton->setChecked(false);
  return;
        }

        TT_CloseSecondarySoundInputDevice(ttInst);

        int samplerate = getSoundDuplexSampleRate(in_dev, out_dev);
        if (samplerate == 0)
  samplerate = out_dev.nDefaultSampleRate;
        if (samplerate <= 0)
  samplerate = in_dev.nDefaultSampleRate;

        AudioPreprocessor preprocessor = {};
        preprocessor.nPreprocessor = NO_AUDIOPREPROCESSOR;
        SoundDeviceEffects effects = {};
        m_secondaryloop = TT_StartSoundLoopbackTestEx(
  inputid, outputid, samplerate, 1, FALSE,
  &preprocessor, &effects);

        if (!m_secondaryloop)
        {
  QMessageBox::critical(this, tr("Escuta do mic secundário"),
                        tr("Não foi possível iniciar a escuta do microfone secundário."));
  ui.secondaryMicListenButton->setChecked(false);
  restoreSecondarySoundInput();
        }
    }
    else
    {
        if (m_secondaryloop)
        {
  TT_CloseSoundLoopbackTest(m_secondaryloop);
  m_secondaryloop = nullptr;
        }
        restoreSecondarySoundInput();
    }
}

'''
)
replace_once(
    "Client/qtTeamTalk/preferencesdlg.cpp",
    '''    setCurrentItemData(ui.inputdevBox, default_inputid);
    setCurrentItemData(ui.outputdevBox, default_outputid);''',
    '''    setCurrentItemData(ui.inputdevBox, default_inputid);
    setCurrentItemData(ui.secondaryInputDevBox, TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL);
    setCurrentItemData(ui.outputdevBox, default_outputid);'''
)

replace_once(
    "Client/qtTeamTalk/preferencesdlg.cpp",
    '''        int inputid = TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL, outputid = TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL;
        if(ui.inputdevBox->count())
  inputid = ui.inputdevBox->itemData(ui.inputdevBox->currentIndex()).toInt();
        if(ui.outputdevBox->count())
  outputid = ui.outputdevBox->itemData(ui.outputdevBox->currentIndex()).toInt();''',
    '''        int inputid = TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL;
        int secondaryinputid = TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL;
        int outputid = TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL;
        if(ui.inputdevBox->count())
  inputid = ui.inputdevBox->itemData(ui.inputdevBox->currentIndex()).toInt();
        if(ui.secondaryInputDevBox->count())
  secondaryinputid = ui.secondaryInputDevBox->itemData(ui.secondaryInputDevBox->currentIndex()).toInt();
        if(ui.outputdevBox->count())
  outputid = ui.outputdevBox->itemData(ui.outputdevBox->currentIndex()).toInt();'''
)
replace_once(
    "Client/qtTeamTalk/preferencesdlg.cpp",
    '''        TT_GetDefaultSoundDevicesEx(getSoundSystem(), &def_inputid, &def_outputid);
        TT_CloseSoundLoopbackTest(m_sndloop);

        SoundSystem oldsndsys''',
    '''        TT_GetDefaultSoundDevicesEx(getSoundSystem(), &def_inputid, &def_outputid);
        TT_CloseSoundLoopbackTest(m_sndloop);
        if (m_secondaryloop)
        {
  TT_CloseSoundLoopbackTest(m_secondaryloop);
  m_secondaryloop = nullptr;
  ui.secondaryMicListenButton->setChecked(false);
        }

        int resolvedPrimaryInput = inputid == SOUNDDEVICEID_DEFAULT ? def_inputid : inputid;
        if (secondaryinputid != TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL &&
  secondaryinputid == resolvedPrimaryInput)
        {
  QMessageBox::information(this, tr("Sound System"),
      tr("O microfone secundário deve ser diferente do microfone principal. O secundário foi desativado."));
  secondaryinputid = TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL;
  setCurrentItemData(ui.secondaryInputDevBox, secondaryinputid);
        }

        SoundSystem oldsndsys'''
)
replace_once(
    "Client/qtTeamTalk/preferencesdlg.cpp",
    '''        ttSettings->setValue(SETTINGS_SOUND_OUTPUTDEVICE_UID, "");
        for(int i=0;i<m_sounddevices.size();i++)
        {
  if(outputid == m_sounddevices[i].nDeviceID)
      ttSettings->setValue(SETTINGS_SOUND_OUTPUTDEVICE_UID,
                           getSoundDeviceUID(m_sounddevices[i]));
        }

        // reinit sound device if anything has changed''',
    '''        ttSettings->setValue(SETTINGS_SOUND_SECONDARY_INPUTDEVICE_UID, "");
        for(int i=0;i<m_sounddevices.size();i++)
        {
  if(secondaryinputid == m_sounddevices[i].nDeviceID)
      ttSettings->setValue(SETTINGS_SOUND_SECONDARY_INPUTDEVICE_UID,
                           getSoundDeviceUID(m_sounddevices[i]));
        }

        ttSettings->setValue(SETTINGS_SOUND_OUTPUTDEVICE_UID, "");
        for(int i=0;i<m_sounddevices.size();i++)
        {
  if(outputid == m_sounddevices[i].nDeviceID)
      ttSettings->setValue(SETTINGS_SOUND_OUTPUTDEVICE_UID,
                           getSoundDeviceUID(m_sounddevices[i]));
        }

        // reinit sound device if anything has changed'''
)
replace_once(
    "Client/qtTeamTalk/preferencesdlg.cpp",
    '''        sndsysinit |= ttSettings->value(SETTINGS_SOUND_INPUTDEVICE, SETTINGS_SOUND_INPUTDEVICE_DEFAULT).toInt() != inputid;
        sndsysinit |= ttSettings->value(SETTINGS_SOUND_OUTPUTDEVICE, SETTINGS_SOUND_OUTPUTDEVICE_DEFAULT).toInt() != outputid;''',
    '''        sndsysinit |= ttSettings->value(SETTINGS_SOUND_INPUTDEVICE, SETTINGS_SOUND_INPUTDEVICE_DEFAULT).toInt() != inputid;
        sndsysinit |= ttSettings->value(SETTINGS_SOUND_SECONDARY_INPUTDEVICE,
                               SETTINGS_SOUND_SECONDARY_INPUTDEVICE_DEFAULT).toInt() != secondaryinputid;
        sndsysinit |= ttSettings->value(SETTINGS_SOUND_OUTPUTDEVICE, SETTINGS_SOUND_OUTPUTDEVICE_DEFAULT).toInt() != outputid;'''
)
replace_once(
    "Client/qtTeamTalk/preferencesdlg.cpp",
    '''        ttSettings->setValueOrClear(SETTINGS_SOUND_INPUTDEVICE, inputid, SETTINGS_SOUND_INPUTDEVICE_DEFAULT);
        ttSettings->setValueOrClear(SETTINGS_SOUND_OUTPUTDEVICE, outputid, SETTINGS_SOUND_OUTPUTDEVICE_DEFAULT);''',
    '''        ttSettings->setValueOrClear(SETTINGS_SOUND_INPUTDEVICE, inputid, SETTINGS_SOUND_INPUTDEVICE_DEFAULT);
        ttSettings->setValueOrClear(SETTINGS_SOUND_SECONDARY_INPUTDEVICE, secondaryinputid,
                          SETTINGS_SOUND_SECONDARY_INPUTDEVICE_DEFAULT);
        ttSettings->setValueOrClear(SETTINGS_SOUND_OUTPUTDEVICE, outputid, SETTINGS_SOUND_OUTPUTDEVICE_DEFAULT);'''
)
