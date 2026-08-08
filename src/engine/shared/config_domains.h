// This file can be included several times.

#ifndef CONFIG_DOMAIN
#error "CONFIG_DOMAIN macro not defined"
#define CONFIG_DOMAIN(Name, ConfigPath, PreviousConfigPath, LegacyConfigPath, HasVars) ;
#endif

CONFIG_DOMAIN(DDNET, "qmclient/settings_ddnet.cfg", nullptr, "settings_ddnet.cfg", true)
CONFIG_DOMAIN(QMCLIENT, "qmclient/settings_qmclient.cfg", "QmClient/settings_qmclient.cfg", "settings_qmclient.cfg", true)
CONFIG_DOMAIN(TCLIENTPROFILES, "qmclient/qmclient_profiles.cfg", "QmClient/qmclient_profiles.cfg", "qmclient_profiles.cfg", false)
CONFIG_DOMAIN(TCLIENTCHATBINDS, "qmclient/qmclient_chatbinds.cfg", "QmClient/qmclient_chatbinds.cfg", "qmclient_chatbinds.cfg", false)
CONFIG_DOMAIN(TCLIENTWARLIST, "qmclient/qmclient_warlist.cfg", "QmClient/qmclient_warlist.cfg", "qmclient_warlist.cfg", false)
