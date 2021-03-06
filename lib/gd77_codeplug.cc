#include "gd77_codeplug.hh"
#include "config.hh"
#include "channel.hh"
#include "utils.hh"
#include "logger.hh"
#include <QDateTime>


#define OFFSET_TIMESTMP     0x00088
#define OFFSET_SETTINGS     0x000e0
#define OFFSET_MSGTAB       0x00128
#define OFFSET_SCANTAB      0x01790
#define OFFSET_BANK_0       0x03780 // Channels 1-128
#define OFFSET_INTRO        0x07540
#define OFFSET_ZONETAB      0x08010
#define OFFSET_BANK_1       0x0b1b0 // Channels 129-1024
#define OFFSET_CONTACTS     0x17620
#define OFFSET_GROUPTAB     0x1d620

#define GET_MSGTAB()        ((msgtab_t*) &radio_mem[OFFSET_MSGTAB])

/* ******************************************************************************************** *
 * Implementation of GD77Codeplug::channel_t
 * ******************************************************************************************** */
bool
GD77Codeplug::channel_t::isValid() const {
  // Channel is enabled/disabled at channel bank
  return true;
}

void
GD77Codeplug::channel_t::clear() {
  memset(name, 0xff, 16);
  _unused25              = 0;
  _unused30              = 0x50;
  _unused36              = 0;
  tx_signaling_syst      = 0;
  _unused38              = 0;
  rx_signaling_syst      = 0;
  _unused40              = 0x16;
  privacy_group          = PRIVGR_NONE;
  emergency_system_index = 0;
  _unused48              = 0;
  emergency_alarm_ack    = 0;
  data_call_conf         = 0;
  private_call_conf      = 0;
  _unused49_1            = 0;
  privacy                = 0;
  _unused49_5            = 0;
  _unused49_7            = 0;
  dcdm                   = 0;
  _unused50_1            = 0;
  _unused50_6            = 0;
  squelch                = SQ_NORMAL;
  bandwidth              = BW_12_5_KHZ;
  talkaround             = 0;
  _unused51_4            = 0;
  vox                    = 0;
  _unused52              = 0;
}

double
GD77Codeplug::channel_t::getRXFrequency() const {
  return decode_frequency(rx_frequency);
}
void
GD77Codeplug::channel_t::setRXFrequency(double freq) {
  rx_frequency = encode_frequency(freq);
}

double
GD77Codeplug::channel_t::getTXFrequency() const {
  return decode_frequency(tx_frequency);
}
void
GD77Codeplug::channel_t::setTXFrequency(double freq) {
  tx_frequency = encode_frequency(freq);
}

QString
GD77Codeplug::channel_t::getName() const {
  return decode_ascii(name, 16, 0xff);
}
void
GD77Codeplug::channel_t::setName(const QString &n) {
  encode_ascii(name, n, 16, 0xff);
}

Signaling::Code
GD77Codeplug::channel_t::getRXTone() const {
  return decode_ctcss_tone_table(ctcss_dcs_receive);
}
void
GD77Codeplug::channel_t::setRXTone(Signaling::Code tone) {
  ctcss_dcs_receive = encode_ctcss_tone_table(tone);
}

Signaling::Code
GD77Codeplug::channel_t::getTXTone() const {
  return decode_ctcss_tone_table(ctcss_dcs_transmit);
}
void
GD77Codeplug::channel_t::setTXTone(Signaling::Code tone) {
  ctcss_dcs_transmit = encode_ctcss_tone_table(tone);
}

Channel *
GD77Codeplug::channel_t::toChannelObj() const {
  QString name = getName();
  double rxF = getRXFrequency();
  double txF = getTXFrequency();
  Channel::Power pwr = (POWER_HIGH == power) ? Channel::HighPower : Channel::LowPower;
  uint timeout = tot*15;
  bool rxOnly = rx_only;
  if (MODE_ANALOG == channel_mode) {
    AnalogChannel::Admit admit;
    switch (admit_criteria) {
      case ADMIT_ALWAYS: admit = AnalogChannel::AdmitNone; break;
      case ADMIT_CH_FREE: admit = AnalogChannel::AdmitFree; break;
      default:
        return nullptr;
    }
    AnalogChannel::Bandwidth bw = (BW_25_KHZ == bandwidth) ? AnalogChannel::BWWide : AnalogChannel::BWNarrow;
    return new AnalogChannel(
          name, rxF, txF, pwr, timeout, rxOnly, admit, squelch,  getRXTone(), getTXTone(),
          bw, nullptr);
  } else if(MODE_DIGITAL == channel_mode) {
    DigitalChannel::Admit admit;
    switch (admit_criteria) {
      case ADMIT_ALWAYS: admit = DigitalChannel::AdmitNone; break;
      case ADMIT_CH_FREE: admit = DigitalChannel::AdmitFree; break;
      case ADMIT_COLOR: admit = DigitalChannel::AdmitColorCode; break;
      default:
        return nullptr;
    }
    DigitalChannel::TimeSlot slot = repeater_slot2 ?
          DigitalChannel::TimeSlot2 : DigitalChannel::TimeSlot1;
    return new DigitalChannel(
          name, rxF, txF, pwr, timeout, rxOnly, admit, colorcode_rx, slot,
          nullptr, nullptr, nullptr, nullptr);
  }

  return nullptr;
}

bool
GD77Codeplug::channel_t::linkChannelObj(
    Channel *c, Config *conf,
    const QHash<int,int> &scan_table, const QHash<int,int> &group_table, const QHash<int, int> &contact_table) const {
  if (c->is<AnalogChannel>()) {
    AnalogChannel *ac = c->as<AnalogChannel>();
    if (scan_list_index && scan_table.contains(scan_list_index))
      ac->setScanList(conf->scanlists()->scanlist(scan_table[scan_list_index]));
  } else {
    DigitalChannel *dc = c->as<DigitalChannel>();
    if (scan_list_index && scan_table.contains(scan_list_index))
      dc->setScanList(conf->scanlists()->scanlist(scan_table[scan_list_index]));
    if (group_list_index && group_table.contains(group_list_index))
      dc->setRXGroupList(conf->rxGroupLists()->list(group_table[group_list_index]));
    if (contact_name_index && contact_table.contains(contact_name_index))
      dc->setTXContact(conf->contacts()->digitalContact(contact_table[contact_name_index]));
  }
  return true;
}

void
GD77Codeplug::channel_t::fromChannelObj(const Channel *c, const Config *conf) {
  clear();

  setName(c->name());
  setRXFrequency(c->rxFrequency());
  setTXFrequency(c->txFrequency());

  power = (Channel::HighPower == c->power()) ? POWER_HIGH : POWER_LOW;
  tot = c->txTimeout()/15;
  rx_only = c->rxOnly() ? 1 : 0;
  bandwidth = BW_12_5_KHZ;

  if (c->is<AnalogChannel>()) {
    const AnalogChannel *ac = c->as<const AnalogChannel>();
    channel_mode = MODE_ANALOG;
    switch (ac->admit()) {
      case AnalogChannel::AdmitNone: admit_criteria = ADMIT_ALWAYS; break;
      case AnalogChannel::AdmitFree: admit_criteria = ADMIT_CH_FREE; break;
      default: admit_criteria = ADMIT_CH_FREE; break;
    }
    bandwidth = (AnalogChannel::BWWide == ac->bandwidth()) ? BW_25_KHZ : BW_12_5_KHZ;
    squelch = SQ_NORMAL; //ac->squelch();
    setRXTone(ac->rxTone());
    setTXTone(ac->txTone());
    scan_list_index = conf->scanlists()->indexOf(ac->scanList())+1;
  } else if (c->is<DigitalChannel>()) {
    const DigitalChannel *dc = c->as<const DigitalChannel>();
    channel_mode = MODE_DIGITAL;
    switch (dc->admit()) {
      case DigitalChannel::AdmitNone: admit_criteria = ADMIT_ALWAYS; break;
      case DigitalChannel::AdmitFree: admit_criteria = ADMIT_CH_FREE; break;
      case DigitalChannel::AdmitColorCode: admit_criteria = ADMIT_COLOR; break;
    }
    repeater_slot2 = (DigitalChannel::TimeSlot1 == dc->timeslot()) ? 0 : 1;
    colorcode_rx = colorcode_tx = dc->colorCode();
    scan_list_index = conf->scanlists()->indexOf(dc->scanList()) + 1;
    group_list_index = conf->rxGroupLists()->indexOf(dc->rxGroupList()) + 1;
    contact_name_index = conf->contacts()->indexOfDigital(dc->txContact()) + 1;
  }
}


/* ******************************************************************************************** *
 * Implementation of GD77Codeplug::grouplist_t
 * ******************************************************************************************** */
QString
GD77Codeplug::grouplist_t::getName() const {
  return decode_ascii(name, 16, 0xff);
}
void
GD77Codeplug::grouplist_t::setName(const QString &n) {
  encode_ascii(name, n, 16, 0xff);
}

RXGroupList *
GD77Codeplug::grouplist_t::toRXGroupListObj() {
  return new RXGroupList(getName());
}

bool
GD77Codeplug::grouplist_t::linkRXGroupListObj(RXGroupList *lst, const Config *conf, const QHash<int, int> &contact_table) const {
  for (int i=0; (i<32) && member[i]; i++) {
    if (! contact_table.contains(member[i])) {
      logDebug() << "Cannot link RX group list '"<< lst->name() <<
                    ": contact index " << member[i] << " is not defined in contact index-table.";
      return false;
    }
    DigitalContact *contact = conf->contacts()->digitalContact(contact_table[member[i]]);
    if (nullptr == contact) {
      logDebug() << "Cannot link RX group list '"<< lst->name() <<
                    ": contact " << contact_table[member[i]] << " is not defined.";
      return false;
    }
    lst->addContact(contact);
  }
  return true;
}

void
GD77Codeplug::grouplist_t::fromRXGroupListObj(const RXGroupList *lst, const Config *conf) {
  setName(lst->name());
  for (int i=0; i<32; i++) {
    if (i < lst->count())
      member[i] = conf->contacts()->indexOfDigital(lst->contact(i))+1;
    else
      member[i] = 0;
  }
}


/* ******************************************************************************************** *
 * Implementation of GD77Codeplug::contact_t
 * ******************************************************************************************** */
GD77Codeplug::contact_t::contact_t() {
  clear();
}

void
GD77Codeplug::contact_t::clear() {
  memset(name, 0xff, 16);
  memset(id, 0x00, 4);
  type = receive_tone = ring_style = valid = 0;
}

bool
GD77Codeplug::contact_t::isValid() const {
  return 0x00 != valid;
}

uint32_t
GD77Codeplug::contact_t::getId() const {
  return decode_dmr_id_bcd(id);
}
void
GD77Codeplug::contact_t::setId(uint32_t num) {
  encode_dmr_id_bcd(id, num);
}

QString
GD77Codeplug::contact_t::getName() const {
  return decode_ascii(name, 16, 0xff);
}
void
GD77Codeplug::contact_t::setName(const QString &n) {
  encode_ascii(name, n, 16, 0xff);
}

DigitalContact *
GD77Codeplug::contact_t::toContactObj() const {
  if (! isValid())
    return nullptr;
  QString name = getName();
  uint32_t id = getId();
  DigitalContact::Type ctype;
  switch (type) {
    case CALL_PRIVATE: ctype = DigitalContact::PrivateCall; break;
    case CALL_GROUP: ctype = DigitalContact::GroupCall; break;
    case CALL_ALL: ctype = DigitalContact::AllCall; break;
  }
  bool rxTone = (receive_tone && ring_style);
  return new DigitalContact(ctype, name, id, rxTone);
}

void
GD77Codeplug::contact_t::fromContactObj(const DigitalContact *cont, const Config *conf) {
  Q_UNUSED(conf);
  valid = 0xff;
  setName(cont->name());
  setId(cont->number());
  switch (cont->type()) {
    case DigitalContact::PrivateCall: type = CALL_PRIVATE; break;
    case DigitalContact::GroupCall:   type = CALL_GROUP; break;
    case DigitalContact::AllCall:     type = CALL_ALL; break;
  }
  if (cont->rxTone())
    receive_tone = ring_style = 1;
}


/* ******************************************************************************************** *
 * Implementation of GD77Codeplug
 * ******************************************************************************************** */
GD77Codeplug::GD77Codeplug(QObject *parent)
  : CodePlug(parent)
{
  addImage("Radioddity GD77 Codeplug");
  image(0).addElement(0x00080, 0x07b80);
  image(0).addElement(0x08000, 0x16300);
}

bool
GD77Codeplug::encode(Config *config, bool update) {
  // set timestamp
  timestamp_t *ts = (timestamp_t *)data(OFFSET_TIMESTMP);
  ts->setNow();

  // pack basic config
  general_settings_t *gs = (general_settings_t*) data(OFFSET_SETTINGS);
  if (! update)
    gs->initDefault();
  gs->setName(config->name());
  gs->setRadioId(config->id());

  intro_text_t *it = (intro_text_t*) data(OFFSET_INTRO);
  it->setIntroLine1(config->introLine1());
  it->setIntroLine2(config->introLine2());

  // Pack channels
  for (int i=0; i<NCHAN; i++) {
    // First, get bank
    bank_t *b;
    if ((i>>7) == 0)
      b = (bank_t*) data(OFFSET_BANK_0);
    else
      b = (((i>>7)-1) + (bank_t *) data(OFFSET_BANK_1));
    channel_t *ch = &b->chan[i % 128];

    // Disable channel if not used
    if (i >= config->channelList()->count()) {
      b->bitmap[(i % 128) / 8] &= ~(1 << (i & 7));
      continue;
    }

    // Construct from Channel object
    ch->clear();
    ch->fromChannelObj(config->channelList()->channel(i), config);

    // Set valid bit.
    b->bitmap[(i % 128) / 8] |= (1 << (i & 7));
  }

  // Pack Zones
  bool pack_zone_a = true;
  for (int i=0, j=0; i<NZONES; i++) {
    zonetab_t *zt = (zonetab_t*) data(OFFSET_ZONETAB);
    zone_t *z = &zt->zone[i];

next:
    if (j >= config->zones()->count()) {
      // Clear valid bit.
      zt->bitmap[i / 8] &= ~(1 << (i & 7));
      continue;
    }

    // Construct from Zone obj
    Zone *zone = config->zones()->zone(j);
    if (pack_zone_a) {
      pack_zone_a = false;
      if (zone->A()->count())
        z->fromZoneObjA(zone, config);
      else
        goto next;
    } else {
      pack_zone_a = true;
      j++;
      if (zone->B()->count())
        z->fromZoneObjB(zone, config);
      else
        goto next;
    }

    // Set valid bit.
    zt->bitmap[i / 8] |= (1 << (i & 7));
  }

  // Pack Scanlists
  for (int i=0; i<NSCANL; i++) {
    scantab_t *st = (scantab_t*) data(OFFSET_SCANTAB);
    scanlist_t *sl = &st->scanlist[i];

    if (i >= config->scanlists()->count()) {
      // Clear valid bit.
      st->valid[i] = 0;
      continue;
    }

    sl->fromScanListObj(config->scanlists()->scanlist(i), config);
    st->valid[i] = 1;
  }

  // Pack contacts
  for (int i=0; i<NCONTACTS; i++) {
    contact_t *ct = (contact_t*) data(OFFSET_CONTACTS + (i)*sizeof(contact_t));
    ct->clear();
    if (i >= config->contacts()->digitalCount())
      continue;
    ct->fromContactObj(config->contacts()->digitalContact(i), config);
  }

  // Pack Grouplists:
  for (int i=0; i<NGLISTS; i++) {
    grouptab_t *gt = (grouptab_t*) data(OFFSET_GROUPTAB);
    grouplist_t *gl = &gt->grouplist[i];
    if (i >= config->rxGroupLists()->count()) {
      gt->nitems1[i] = 0;
      continue;
    }
    // Enable group list
    gt->nitems1[i] = std::min(15,config->rxGroupLists()->list(i)->count()) + 1;
    gl->fromRXGroupListObj(config->rxGroupLists()->list(i), config);
  }

  return true;
}

bool
GD77Codeplug::decode(Config *config) {
  // Clear config object
  config->reset();

  /* Unpack general config */
  general_settings_t *gs = (general_settings_t*) data(OFFSET_SETTINGS);
  if (nullptr == gs) {
    _errorMessage = QString("%1(): Cannot access general settings memory!")
        .arg(__func__);
    return false;
  }

  config->setId(gs->getRadioId());
  config->setName(gs->getName());
  intro_text_t *it = (intro_text_t*) data(OFFSET_INTRO);
  config->setIntroLine1(it->getIntroLine1());
  config->setIntroLine2(it->getIntroLine2());

  /* Unpack Contacts */
  QHash<int,int> contact_table;
  for (int i=0; i<NCONTACTS; i++) {
    contact_t *ct = (contact_t *) data(OFFSET_CONTACTS+i*sizeof(contact_t));
    if (nullptr == ct) {
      _errorMessage = QString("%1(): Cannot access contact memory at index %2!")
          .arg(__func__).arg(i);
      return false;
    }
    // If contact is disabled
    if (! ct->isValid())
      continue;
    logDebug() << "Contact " << i << " enabled.";
    if (DigitalContact *cont = ct->toContactObj()) {
      logDebug() << "Contact at index " << i+1 << " mapped to "
                 << config->contacts()->count();
      contact_table[i+1] = config->contacts()->digitalCount();
      config->contacts()->addContact(cont);
    } else {
      _errorMessage = QString("%1(): Cannot decode codeplug: Invalid contact at index %2.")
          .arg(__func__).arg(i);
      return false;
    }
  }

  /* Unpack RX Group Lists */
  QHash<int, int> group_table;
  for (int i=0; i<NGLISTS; i++) {
    grouptab_t *gt = (grouptab_t*) data(OFFSET_GROUPTAB);
    if (nullptr == gt) {
      _errorMessage = QString("%1(): Cannot access group list table memory!")
          .arg(__func__);
      return false;
    }

    if (0 == gt->nitems1[i])
      continue;

    grouplist_t *gl = &gt->grouplist[i];
    if (nullptr == gl) {
      _errorMessage = QString("%1(): Cannot access group list memory at index %2!")
          .arg(__func__).arg(i);
      return false;
    }

    RXGroupList *list = gl->toRXGroupListObj();
    if (list) {
      logDebug() << "RX group list at index " << i+1 << " mapped to "
                 << config->rxGroupLists()->count();
      group_table[i+1] = config->rxGroupLists()->count();
      config->rxGroupLists()->addList(list);
    } else {
      _errorMessage = QString("%1(): Cannot decode codeplug: Invalid RX group-list at index %2.")
          .arg(__func__).arg(i);
      return false;
    }

    if(! gl->linkRXGroupListObj(list, config, contact_table)) {
      _errorMessage = QString("%1(): Cannot decode codeplug: Cannot link RX group list at index %2.")
          .arg(__func__).arg(i);
      return false;
    }
  }

  /* Unpack Channels */
  QHash<int, int> channel_table;
  for (int i=0; i<NCHAN; i++) {
    // First, get bank
    bank_t *b;
    if ((i>>7) == 0)
      b = (bank_t*) data(OFFSET_BANK_0);
    else
      b = (((i>>7)-1) + (bank_t*) data(OFFSET_BANK_1));
    if (nullptr == b) {
      _errorMessage = QString("%1(): Cannot access channel bank at index %2!")
          .arg(__func__).arg(i);
      return false;
    }
    // If channel is disabled -> skip
    if (! ((b->bitmap[i % 128 / 8] >> (i & 7)) & 1) )
      continue;

    // finally, get channel
    channel_t *ch = &b->chan[i % 128];
    if (! ch){
      _errorMessage = QString("%1(): Cannot access channel at index %2!")
          .arg(__func__).arg(i);
      return false;
    }

    if (Channel *chan = ch->toChannelObj()) {
      logDebug() << "Map channel index " << i << " to " << config->channelList()->count();
      channel_table[i+1] = config->channelList()->count();
      config->channelList()->addChannel(chan);
    } else {
      _errorMessage = QString("%1(): Cannot unpack codeplug: Invalid channel at index %2!")
          .arg(__func__).arg(i);
      return false;
    }
  }

  /* Unpack Zones */
  for (int i=0; i<NZONES; i++) {
    zonetab_t *zt = (zonetab_t*) data(OFFSET_ZONETAB);
    if (! zt){
      _errorMessage = QString("%1(): Cannot access zone table memory.")
          .arg(__func__);
      return false;
    }
    // if zone is disabled
    if (! (zt->bitmap[i / 8] >> (i & 7) & 1) )
      continue;
    // get zone_t
    zone_t *z = &zt->zone[i];
    if (! z){
      _errorMessage = QString("%1(): Cannot access zone at index %2")
          .arg(__func__).arg(i);
      return false;
    }

    // Create & link zone obj
    Zone *zone = z->toZoneObj();
    if (nullptr == zone) {
      _errorMessage = QString("%1(): Cannot unpack codeplug: Invalid zone at index %2")
          .arg(__func__).arg(i);
      return false;
    }
    if (! z->linkZoneObj(zone, config, channel_table)) {
      _errorMessage = QString("%1(): Cannot unpack codeplug: Cannot link zone at index %2")
          .arg(__func__).arg(i);
      return false;
    }
    config->zones()->addZone(zone);
  }

  /* Unpack Scan lists */
  QHash<int, int> scan_table;
  for (int i=0; i<NSCANL; i++) {
    scantab_t *st = (scantab_t*) data(OFFSET_SCANTAB);
    if (! st){
      _errorMessage = QString("%1(): Cannot access scanlist table memory!")
          .arg(__func__);
      return false;
    }
    if (! st->valid[i])
      continue;

    scanlist_t *sl = &st->scanlist[i];
    if (! sl){
      _errorMessage = QString("%1(): Cannot access scan list at index %2")
          .arg(__func__).arg(i);
      return false;
    }

    ScanList *scan = sl->toScanListObj();
    if (nullptr == scan) {
      _errorMessage = QString("%1(): Cannot unpack codeplug: Invalid scanlist at index %2")
          .arg(__func__).arg(i);
      return false;
    }

    if (! sl->linkScanListObj(scan, config, channel_table)) {
      _errorMessage = QString("%1(): Cannot unpack codeplug: Cannot link scanlist at index %2")
          .arg(__func__).arg(i);
      return false;
    }

    logDebug() << "Scan list at index " << i+1 << " mapped to "
               << config->scanlists()->count();
    scan_table[i+1] = config->scanlists()->count();
    config->scanlists()->addScanList(scan);
  }

  /*
   * Link Channels -> ScanLists
   */
  for (int i=0; i<NCHAN; i++) {
    // First, get bank
    bank_t *b;
    if ((i>>7) == 0)
      b = (bank_t*) data(OFFSET_BANK_0);
    else
      b = (((i>>7)-1) + (bank_t*) data(OFFSET_BANK_1));
    // If channel is disabled
    if (! ((b->bitmap[i % 128 / 8] >> (i & 7)) & 1) )
      continue;
    // finally, get channel
    channel_t *ch = &b->chan[i % 128];
    if (! ch->linkChannelObj(config->channelList()->channel(channel_table[i]),
                             config, scan_table, group_table, contact_table))
    {
      _errorMessage = QString("%1(): Cannot unpack codeplug: Cannot link channel at index %2")
          .arg(__func__).arg(i);
      return false;
    }
  }

  return true;
}
