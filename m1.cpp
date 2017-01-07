#include <QtConcurrent>
#include "m1ui.h"
#include "m1.h"
#include "prototype.h"
#include "utils.h"
#include "mainwindow.h"
#include "mameopt.h"

M1UI::M1UI(QWidget *parent) :
	QDockWidget(parent)
{
	setupUi(this);
	setObjectName("M1");
	setWindowTitle("M1");

	const QStringList m1Headers = (QStringList()
		<< "#" << tr("Name") << tr("Len"));
	twSongList->setHeaderLabels(m1Headers);
	twSongList->header()->moveSection(2, 1);

	//fixme: should be dynamic
	const QStringList langs = (QStringList()
		<< "en" << "jp");
	cmbLang->addItems(langs);

	QString m1Language = pGuiSettings->value("m1_language").toString();
	int sel = langs.indexOf(m1Language);
	if (sel < 0)
		sel = 0;
	cmbLang->setCurrentIndex(sel);

/*
	if (m1Language == "jp")
	{
		QFont font;
		font.setFamily("MS Gothic");
		font.setFixedPitch(true);
		twSongList->setFont(font);
		lblTrackName->setFont(font);
	}
*/
}

void M1UI::init()
{
	connect(btnPlay, SIGNAL(pressed()), &win->m1Core->m1Thread, SLOT(play()));
	connect(twSongList, SIGNAL(itemActivated(QTreeWidgetItem*, int)), &win->m1Core->m1Thread, SLOT(play(QTreeWidgetItem*, int)));
	connect(btnStop, SIGNAL(pressed()), &win->m1Core->m1Thread, SLOT(stop()));
	connect(cmbLang, SIGNAL(currentIndexChanged(const QString &)), win->m1Core, SLOT(updateList(const QString &)));

	setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding));
//	win->addDockWidget(static_cast<Qt::DockWidgetArea>(Qt::RightDockWidgetArea), this);
}

M1Core::M1Core(QObject *parent) :
	QObject(parent),
	max_games(0),
	isHex(false),
	available(false)
{
}

M1Core::~M1Core()
{
	m1snd_shutdown();
}

void M1Core::init()
{
	connect(&m1Watcher, SIGNAL(finished()), this, SLOT(postInit()));
	QFuture<void> future = QtConcurrent::run(this, &M1Core::loadLib);
	m1Watcher.setFuture(future);
}

void M1Core::loadLib()
{
	//set current dir to m1, so that m1.xml and list could be loaded
	m1_dir = utils->getPath(pGuiSettings->value("m1_directory", "bin/m1").toString());
//	win->log(m1_dir);

	win->logStatus(tr("Loading M1, please wait..."));
	QDir::setCurrent(m1_dir);
	
	QLibrary m1Lib(0);
	m1Lib.setFileName("m1");

	//resolve symbols from m1 lib
	if (m1Lib.load())
	{
		win->enableCtrls(false);

		m1snd_init = (fp_m1snd_init)m1Lib.resolve("m1snd_init");
		m1snd_run = (fp_m1snd_run)m1Lib.resolve("m1snd_run");
		m1snd_shutdown = (fp_m1snd_shutdown)m1Lib.resolve("m1snd_shutdown");
		m1snd_setoption =  (fp_m1snd_setoption)m1Lib.resolve("m1snd_setoption");
		m1snd_get_info_int = (fp_m1snd_get_info_int)m1Lib.resolve("m1snd_get_info_int");
		m1snd_get_info_str = (fp_m1snd_get_info_str)m1Lib.resolve("m1snd_get_info_str");
		m1snd_set_info_int = (fp_m1snd_set_info_int)m1Lib.resolve("m1snd_set_info_int");
		m1snd_set_info_str = (fp_m1snd_set_info_str)m1Lib.resolve("m1snd_set_info_str");

		if (m1snd_init &&
			m1snd_run &&
			m1snd_shutdown &&
			m1snd_setoption &&
			m1snd_get_info_int &&
			m1snd_get_info_str &&
			m1snd_set_info_int &&
			m1snd_set_info_str)
		{
			m1snd_setoption(M1_OPT_RETRIGGER, 0);
			m1snd_setoption(M1_OPT_WAVELOG, 0);
			m1snd_setoption(M1_OPT_NORMALIZE, 1);
			m1snd_setoption(M1_OPT_LANGUAGE, M1_LANG_EN);
//			m1snd_setoption(M1_OPT_USELIST, 1);
			m1snd_setoption(M1_OPT_RESETNORMALIZE, 0);

			m1snd_init(0, m1ui_message);

			//if m1 is not loaded successfully, max_games keeps 0
			max_games = m1snd_get_info_int(M1_IINF_TOTALGAMES, 0);
			version = QString(m1snd_get_info_str(M1_SINF_COREVERSION, 0));
		}
		
		win->enableCtrls(true);
	}

	//restore current dir
	QDir::setCurrent(currentAppDir);

	win->logStatus("");
}

void M1Core::postInit()
{
	if (max_games > 0)
	{
		available = true;

		win->m1UI->init();
		updateList();
		win->setVersion();

		QAction *actionM1UI = win->m1UI->toggleViewAction();
		win->menuView->insertAction(win->actionVerticalTabs, actionM1UI);
		
		win->log("m1 loaded");
	}
}

void M1Core::updateList(const QString &)
{
	QString gameName = currentGame;

	QString fileName = QString("%1lists/%2/%3%4")
		.arg(m1_dir)
		.arg(win->m1UI->cmbLang->currentText())
		.arg(gameName)
		.arg(".lst");
	QFile datFile(fileName);

	QString buf = "";

	win->m1UI->twSongList->clear();

	if (datFile.open(QFile::ReadOnly | QFile::Text))
	{
		QTextStream in(&datFile);
		in.setCodec("UTF-8");

//		bool isFound, recData = false;
		QString line;

		QList<QTreeWidgetItem *> items;
		//valid track line
		QRegExp rxTrack("([#$][\\da-fA-F]+)\\s+(.*)");
		QRegExp rxTime("<time=\"([\\d.:]*)\">");
		int pos;

		do
		{
			line = in.readLine();
			if (line.startsWith("#") || line.startsWith("$"))
			{
				pos = rxTrack.indexIn(line);
				QStringList list = rxTrack.capturedTexts();

				if (pos > -1)
				{
					//remove the complete matching \0
					list.removeFirst();
					//look for track length
					pos = rxTime.indexIn(list[1]);
					if (pos > -1)
					{
						list[1] = list[1].left(pos);
						list.append(rxTime.cap(1).trimmed());
					}

					items.append(new QTreeWidgetItem(win->m1UI->twSongList, list));
				}
			}
			else
			{
				QStringList list;
				list.append("");
				list.append(line.trimmed());

				QTreeWidgetItem *item = new QTreeWidgetItem(win->m1UI->twSongList, list);
				item->setDisabled(true);

				items.append(item);
			}
		}
		while (!line.isNull());

        win->m1UI->twSongList->header()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
        win->m1UI->twSongList->header()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
        win->m1UI->twSongList->header()->setSectionResizeMode(1,QHeaderView::Stretch);
	}
}

int M1Core::m1ui_message(void *pthis, int message, char *txt, int iparm)
{
//	win->log(QString("M1 callback %1, %2").arg(message).arg(txt));
	switch (message)
	{
		case M1_MSG_HARDWAREDESC:
		{
			GameInfo *gameInfo = pMameDat->games[currentGame];
			
			win->m1UI->lblTrackInfo->setText(
				QString("<b>Manufacturer: </b>%1 %2<br>"
						"<b>Hardware: </b>%3")
						.arg(gameInfo->year)
						.arg(gameInfo->manufacturer)
						.arg(txt));
			break;
		}
		case M1_MSG_ROMLOADERR:
			win->log("M1 ERR: ROMLOADERR");
			win->m1Core->m1Thread.stop();
			break;

		case M1_MSG_STARTINGSONG:
			win->log(QString("M1 INF: STARTINGSONG %1").arg(iparm));
			break;

		case M1_MSG_ERROR:
			win->log(QString("M1 ERR: %1").arg(txt));
			win->m1Core->m1Thread.stop();
			break;

		case M1_MSG_HARDWAREERROR:
			win->log("M1 ERR: HARDWAREERROR");
			win->m1Core->m1Thread.stop();
			break;

		case M1_MSG_MATCHPATH:
		{
//			win->log(QString("M1 INF: MATCHPATH: %1").arg(txt));

			//test if rom's available
			QStringList dirpaths = mameOpts["rompath"]->currvalue.split(";");
			foreach (QString _dirpath, dirpaths)
			{
				QDir dir(_dirpath);
				QString dirpath = utils->getPath(_dirpath);

				QString path = dirpath + currentGame + ZIP_EXT;
				QFile file(path);
				if (file.exists())
				{
					strcpy(txt, qPrintable(path));
					win->m1Core->m1snd_set_info_str(M1_SINF_SET_ROMPATH, txt, 0, 0, 0);
	//				win->log(QString("M1 INF: MATCHPATH2: %1").arg(txt));
					return 1;
				}
			}

			win->log("M1 ERR: MATCHPATH");
			return 0;
		}

		case M1_MSG_GETWAVPATH:
		{
			int song = win->m1Core->m1snd_get_info_int(M1_IINF_CURCMD, 0);
			int game = win->m1Core->m1snd_get_info_int(M1_IINF_CURGAME, 0);
	
			sprintf(txt, "%s%s%04d.wav", "", win->m1Core->m1snd_get_info_str(M1_SINF_ROMNAME, game), song);
//			win->log(txt);
			break;
		}
	}
	return 0;
}

M1Thread::M1Thread(QObject *parent) :
	QThread(parent),
	gameNum(-1),
	cmdNum(0)
{
}

M1Thread::~M1Thread()
{
	done = true;
	wait();
}

void M1Thread::pause(){}
void M1Thread::prev(){}
void M1Thread::next(){}
void M1Thread::record(){}

void M1Thread::stop()
{
	//stop current thread
	if (isRunning())
	{
		cancel = true;
		wait();
	}
}

void M1Thread::play(QTreeWidgetItem*, int)
{
	QList<QTreeWidgetItem *> selectedItems = win->m1UI->twSongList->selectedItems();
	if (selectedItems.isEmpty())
		return;

	bool ok;
	QString cmdStr = selectedItems[0]->text(0).trimmed();

	if (cmdStr.startsWith("$"))
	{
		cmdStr = cmdStr.remove(0, 1);
		cmdNum = cmdStr.toInt(&ok, 16);
		win->m1UI->lcdNumber->setHexMode();
	}
	else
	{
		cmdStr = cmdStr.remove(0, 1);
		cmdNum = cmdStr.toInt(&ok);
		win->m1UI->lcdNumber->setDecMode();
	}

	if (!ok)
		return;

	win->m1UI->lcdNumber->display(cmdStr);
	win->m1UI->lblTrackName->setText(selectedItems[0]->text(1));

	QMutexLocker locker(&mutex);

	for (int curgame = 0; curgame < win->m1Core->max_games; curgame++)
	{
		if (currentGame == win->m1Core->m1snd_get_info_str(M1_SINF_ROMNAME, curgame))
		{
			gameNum = curgame;
			break;
		}
	}

	stop();

	cancel = false;
	done = false;
	start(NormalPriority);
}

void M1Thread::run()
{
	/*
	int maxtracks = m1snd_get_info_int(M1_IINF_TRACKS, curgame);

	for (int track = 0; track < maxtracks; track++)
	{
		int cmd = m1snd_get_info_int(M1_IINF_TRACKCMD, (track << 16) | curgame);
		win->log(QString::fromUtf8(m1snd_get_info_str(M1_SINF_TRKNAME, (cmd << 16) | curgame)));
	}
	*/
	
	win->m1Core->m1snd_setoption(M1_OPT_DEFCMD, cmdNum);
	win->m1Core->m1snd_run(M1_CMD_GAMEJMP, gameNum);
//	win->log(QString("play1: %1").arg(m1->m1snd_get_info_int(M1_IINF_CURSONG, 0)));

	while(!cancel)
		win->m1Core->m1snd_run(M1_CMD_IDLE, 0);

	//have to call this to stop sound
	win->m1Core->m1snd_run(M1_CMD_GAMEJMP, gameNum);
	win->m1Core->m1snd_run(M1_CMD_STOP, 0);
}

