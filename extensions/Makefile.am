AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/librb/include $(LTDLINCL)
AM_LDFLAGS  = -module -export-dynamic -avoid-version -no-undefined -shared
AM_LDFLAGS += -export-symbols-regex _mheader
LIBS += $(top_srcdir)/ircd/libircd.la

extensiondir=@moduledir@/extensions

extension_LTLIBRARIES =		\
  cap_oper.la			\
  cap_realhost.la		\
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
  drain.la			\
  extb_account.la		\
  extb_canjoin.la		\
  extb_channel.la		\
  extb_guest.la		\
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
  hide_uncommon_channels.la	\
  hurt.la			\
  identify_msg.la		\
  invex_regonly.la		\
  invite_notify.la		\
  ip_cloaking.la		\
  ip_cloaking_old.la		\
  ip_cloaking_3.0.la		\
  ip_cloaking_4.0.la		\
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
  m_remove.la			\
  m_sendbans.la			\
  m_shedding.la			\
  m_webirc.la			\
  no_kill_services.la		\
  no_locops.la			\
  no_oper_invis.la		\
  override.la			\
  override_kick_immunity.la	\
  restrict-unauthenticated.la	\
  sasl_usercloak.la		\
  sno_channelcreate.la		\
  sno_farconnect.la		\
  sno_globalnickchange.la	\
  sno_globaloper.la		\
  umode_hide_idle_time.la	\
  umode_noctcp.la		\
  example_module.la

if HAVE_HYPERSCAN
    extension_LTLIBRARIES += filter.la
endif
