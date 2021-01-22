AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/librb/include $(LTDLINCL)
AM_LDFLAGS  = -module -export-dynamic -avoid-version -no-undefined -shared
AM_LDFLAGS += -export-symbols-regex _mheader
LIBS += $(top_srcdir)/ircd/libircd.la
if MINGW
LIBS += $(top_srcdir)/librb/src/librb.la
endif

extensiondir=@moduledir@/extensions

extension_LTLIBRARIES =		\
  chantype_dummy.la		\
  chm_adminonly.la		\
  chm_operonly.la		\
  chm_insecure.la		\
  chm_nonotice.la		\
  chm_operpeace.la		\
  chm_regmsg.la			\
  chm_sslonly.la		\
  createauthonly.la		\
  createoperonly.la		\
  extb_account.la		\
  extb_canjoin.la		\
  extb_channel.la		\
  extb_hostmask.la		\
  extb_oper.la			\
  extb_server.la		\
  extb_ssl.la			\
  extb_realname.la		\
  extb_usermode.la		\
  extb_extgecos.la		\
  extb_combi.la			\
  force_user_invis.la		\
  helpops.la			\
  hurt.la			\
  invite_notify.la		\
  ip_cloaking.la		\
  ip_cloaking_old.la		\
  ip_cloaking_3.0.la		\
  ip_cloaking_4.0.la		\
  override.la			\
  override_kick_immunity.la	\
  restrict-unauthenticated.la	\
  sno_channelcreate.la		\
  sno_farconnect.la		\
  sno_globalkline.la		\
  sno_globalnickchange.la	\
  sno_globaloper.la		\
  umode_noctcp.la		\
  m_adminwall.la		\
  m_echotags.la			\
  m_extendchans.la		\
  m_findforwards.la		\
  m_identify.la			\
  m_locops.la			\
  m_mkpasswd.la			\
  m_ojoin.la			\
  m_okick.la			\
  m_omode.la			\
  m_opme.la			\
  m_sendbans.la			\
  m_webirc.la			\
  m_remove.la			\
  hide_uncommon_channels.la	\
  no_kill_services.la		\
  no_locops.la			\
  no_oper_invis.la		\
  sasl_usercloak.la		\
  drain.la			\
  identify_msg.la		\
  cap_realhost.la		\
  example_module.la

if HAVE_HYPERSCAN
    extension_LTLIBRARIES += filter.la
endif
