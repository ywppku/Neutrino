#include "holderGUI.h"
#include <QImageReader>
#include <QFileDialog>
#include <QListWidgetItem>
#include <QTreeWidgetItem>

#include "graphics/nView.h"
#include "genericPan/genericPan.h"

holderGUI::holderGUI() : QMainWindow() ,
    listPhys(new physList(this)),
    treeViewers(new viewerTree(this)),
    treePans(new panTree(this))
{
	setupUi(this);

	QGridLayout *tab1_layout=new QGridLayout(tab_phys);
	tab1_layout->setContentsMargins(0, 0, 0, 0);
	tab1_layout->addWidget(listPhys);

	QGridLayout *tab2_layout=new QGridLayout(tab_viewers);
	tab2_layout->setContentsMargins(0, 0, 0, 0);
	tab2_layout->addWidget(treeViewers);

	QGridLayout *tab3_layout=new QGridLayout(tab_pans);
	tab3_layout->setContentsMargins(0, 0, 0, 0);
	tab3_layout->addWidget(treePans);

	show();
}

void holderGUI::on_actionViewer_triggered() {
	QPointer<genericPan> physViewer = new genericPan(this);
	physViewer->show();

	nView *my_view = new nView(physViewer);

	physViewer->setCentralWidget(my_view);
	physViewer->resize(400,400);

	connect(listPhys, SIGNAL(nPhysDSelected(nPhysD*)), my_view, SLOT(showPhys(nPhysD*)));
	connect(listPhys, SIGNAL(addPhys(nPhysD*)), my_view, SLOT(showPhys(nPhysD*)));

	for (auto& img: allPhys()) {
		my_view->showPhys(img);
	}

	physViewer->show();
}

std::vector<nPhysD*> holderGUI::allPhys() {
	std::vector<nPhysD*> retvec;
	for(int i = 0; i < listPhys->count(); ++i) {
		nPhysD *my_phys=listPhys->item(i)->data(1).value<nPhysD*>();
		if (my_phys) {
			retvec.push_back(my_phys);
		}
	}
	return retvec;
}

void holderGUI::saveDefaults(){
	QSettings my_set("neutrino","");
	my_set.beginGroup("nPreferences");
	my_set.setValue("geometry", pos());

	my_set.beginGroup("Properties");
	foreach(QByteArray ba, dynamicPropertyNames()) {
		if(ba.startsWith("NeuSave")) {
			my_set.setValue(ba, property(ba));
		}
	}
	my_set.endGroup();
	my_set.endGroup();
}

void holderGUI::loadDefaults(){
	QSettings my_set("neutrino","");
	my_set.beginGroup("nPreferences");
	move(my_set.value("geometry",pos()).toPoint());

	if (my_set.childGroups().contains("Properties")) {
		my_set.beginGroup("Properties");
		foreach(QString my_key, my_set.allKeys()) {
			setProperty(my_key.toUtf8().constData(), my_set.value(my_key));
			qDebug() << my_key;
		}
		my_set.endGroup();
	}

	my_set.endGroup();
}

void holderGUI::closeEvent (QCloseEvent *e) {
	saveDefaults();
	e->accept();
}


void holderGUI::dragEnterEvent(QDragEnterEvent *e)
{
	Ui_holderGUI::statusBar->showMessage(tr("Drop content"), 2000);
	e->acceptProposedAction();
}

void holderGUI::dragMoveEvent(QDragMoveEvent *e)
{
	e->accept();
}

void holderGUI::dropEvent(QDropEvent *e) {
	qDebug() << "here";
	if (e->mimeData()->hasUrls()) {
		e->acceptProposedAction();
		foreach (QUrl qurl, e->mimeData()->urls()) {
			openFiles(QStringList(qurl.toLocalFile()));
		}
	} else {
		qDebug() << "here";
	}
}


void holderGUI::logging(QString msg) {
	qDebug() << sender() << ":" << msg;
}

void holderGUI::on_actionOpen_triggered() {
	QString formats("");
	formats+="Neutrino Images (*.txt *.neu *.neus *.tif *.tiff *.hdf *.sif *.b16 *.spe *.pcoraw *.img *.raw *.fits *.inf *.gz);;";
	formats+="Images (";
	foreach (QByteArray format, QImageReader::supportedImageFormats() ) {
		formats+="*."+format+" ";
	}
	formats.chop(1);
	formats+=");;";
	formats+=("Any files (*)");

	QStringList fnames = QFileDialog::getOpenFileNames(this,tr("Open Image(s)"),property("NeuSave-fileOpen").toString(),formats);
	openFiles(fnames);
}

std::vector<nPhysD*> holderGUI::fileOpen(QString fname) {

	setProperty("NeuSave-fileOpen", fname);
	std::vector<nPhysD*> retlist;

	for (auto &my_phys : phys_open(fname.toStdString())) {
		DEBUG(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>")
		nPhysD* my_nphys=new nPhysD(my_phys);
		retlist.push_back(my_nphys);
	}

	return retlist;
}

std::vector<nPhysD*> holderGUI::openFiles(QStringList fnames) {
	std::vector<nPhysD*> retlist;
	foreach (QString fname, fnames) {
		for (auto& img: fileOpen(fname)) {
			retlist.push_back(img);
		}
	}
	return retlist;
}
