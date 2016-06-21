/*
 *
 *    Copyright (C) 2013 Alessandro Flacco, Tommaso Vinci All Rights Reserved
 * 
 *    This file is part of neutrino.
 *
 *    Neutrino is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Neutrino is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public License
 *    along with neutrino.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    Contact Information: 
 *	Alessandro Flacco <alessandro.flacco@polytechnique.edu>
 *	Tommaso Vinci <tommaso.vinci@polytechnique.edu>
 *
 */
#include "nLine.h"
#include "neutrino.h"
#include <iostream>

//https://github.com/graiola/spline
#include "spline.h"

nLine::~nLine() {
	foreach (QGraphicsEllipseItem* item, ref) {
		delete item;
	}
}

nLine::nLine(neutrino *nparent) : QGraphicsObject()
{

    if (nparent) {
        nparent->my_s.addItem(this);
        setParent(nparent);
        int num=nparent->property("numLine").toInt()+1;
        nparent->setProperty("numLine",num);
        setProperty("numLine",num);
        setToolTip(tr("line")+QString::number(num));
        connect(nparent, SIGNAL(mouseAtMatrix(QPointF)), this, SLOT(movePoints(QPointF)));
        connect(nparent->my_w.my_view, SIGNAL(zoomChanged(double)), this, SLOT(zoomChanged(double)));
        connect(nparent, SIGNAL(bufferChanged(nPhysD*)), this, SLOT(bufferChanged(nPhysD*)));

        zoom=nparent->getZoom();

        if (nparent->currentBuffer) {

	    		setPos(nparent->currentBuffer->get_origin().x(),nparent->currentBuffer->get_origin().y());
		}

	} else {
		setToolTip(tr("line"));
	}

	setAcceptHoverEvents(true);
	setFlag(QGraphicsItem::ItemIsSelectable);
	setFlag(QGraphicsItem::ItemIsFocusable);
	setProperty("parentPan",QString(""));
	setProperty("parentPanControlLevel",0);

	nodeSelected=-1;

	nWidth=1.0;
	nSizeHolder=5.0;
	numPoints=300;
	colorLine=QColor(Qt::black);
	colorHolder=QColor(255,0,0,200);

	bezier=false;
	forceMonotone = false;
	forceInverseOrdering = false;
	closedLine=false;
	antialias=false;

	setOrder(0.0);
	// PADELLA

	my_pad.setWindowTitle(toolTip());
	my_pad.setWindowIcon(QIcon(":line"));
	my_w.setupUi(&my_pad);

	connect(my_w.actionLoadPref, SIGNAL(triggered()), this, SLOT(loadSettings()));
	connect(my_w.actionSavePref, SIGNAL(triggered()), this, SLOT(saveSettings()));
	connect(my_w.actionSaveClipboard, SIGNAL(triggered()), this, SLOT(copy_clip()));
	connect(my_w.actionSaveTxt, SIGNAL(triggered()), this, SLOT(export_txt()));
	connect(my_w.actionBezier, SIGNAL(triggered()), this, SLOT(toggleBezier()));
	connect(my_w.actionClosedLine, SIGNAL(triggered()), this, SLOT(toggleClosedLine()));
	connect(my_w.actionAntialias, SIGNAL(triggered()), this, SLOT(toggleAntialias()));

	my_w.spinWidth->setValue(nWidth);
	my_w.spinDepth->setValue(zValue());
	my_w.colorLabel->setPalette(QPalette(colorLine));

	connect(my_w.name, SIGNAL(textChanged(QString)), this, SLOT(changeToolTip(QString)));

	my_w.name->setText(toolTip());
	my_w.spinSizeHolder->setValue(nSizeHolder);
	my_w.cutPoints->setValue(numPoints);
	my_w.colorHolderLabel->setPalette(QPalette(colorHolder));

	connect(my_w.addPoint, SIGNAL(released()),this, SLOT(addPoint()));
	connect(my_w.removeRow, SIGNAL(released()),this, SLOT(removePoint()));
	connect(my_w.copyPoints, SIGNAL(released()),this, SLOT(copyPoints()));
	connect(my_w.exportTxt, SIGNAL(released()),this, SLOT(export_txt_points()));

	connect(my_w.spinWidth, SIGNAL(valueChanged(double)), this, SLOT(setWidthF(double)));
	connect(my_w.spinDepth, SIGNAL(valueChanged(double)), this, SLOT(setOrder(double)));
	connect(my_w.colorButton, SIGNAL(pressed()), this, SLOT(changeColor()));
	connect(my_w.colorHolderButton, SIGNAL(pressed()), this, SLOT(changeColorHolder()));
	connect(my_w.spinSizeHolder, SIGNAL(valueChanged(double)), this, SLOT(sizeHolder(double)));
	connect(my_w.points, SIGNAL(itemChanged(QTableWidgetItem * )), this, SLOT(tableUpdated(QTableWidgetItem * )));

	connect(my_w.cutPoints, SIGNAL(valueChanged(int)), this, SLOT(setNumPoints(int)));

	connect(my_w.tabWidget, SIGNAL(currentChanged(int)), this, SLOT(updatePlot()));

}

void nLine::setParentPan(QString winname, int level) {
	my_w.name->setText(winname+"Line");
	setProperty("parentPan",winname);
	setProperty("parentPanControlLevel",level);
	if (level>0) {
		my_w.name->setReadOnly(true);
		disconnect(my_w.name, SIGNAL(textChanged(QString)), this, SLOT(changeToolTip(QString)));
	}
	if (level>1) {
		sizeHolder(0.0);
	}
	if (level>2) {
		my_w.cutPoints->setValue(1);
	}
}


void nLine::setPoints(QPolygonF my_poly) {
	while (ref.size()>my_poly.size()) {
		prepareGeometryChange();
		delete ref.at(0);
		ref.removeAt(0);
		my_w.points->removeRow(0);
		update();
	}
	while (ref.size()<my_poly.size()) appendPoint();
	for (int i=0; i<ref.size();i++) {
		changeP(i, my_poly.at(i));
	}
	moveRef.clear();
}

QPolygonF nLine::getPoints() {
	QPolygonF my_poly;
	foreach (QGraphicsEllipseItem* r, ref) {
		my_poly << r->pos();
	}
	return my_poly;
}

void nLine::bufferChanged(nPhysD* my_phys) {    
	if (my_phys) {
		// qui si definisce la posizione dell'origine (quindi non deve dipendere dalla scala)
		setPos(my_phys->get_origin().x(),my_phys->get_origin().y());
	} else {
		setPos(0,0);
	}
	updatePlot();
}

QPolygonF nLine::getLine(int np) {
	return mapToScene(poly(np));
}

void nLine::interactive ( ) {
	showMessage(tr("Click for first point, press Esc to finish"));
	connect(parent()->my_w.my_view, SIGNAL(mouseReleaseEvent_sig(QPointF)), this, SLOT(addPointAfterClick(QPointF)));
	appendPoint();
}

void nLine::addPointAfterClick (QPointF) {
	showMessage(tr("Point added, press ESC to finish"));
	moveRef.clear();
	appendPoint();
//    moveRef.erase(moveRef.begin(),moveRef.end()-1);
}

void nLine::mousePressEvent ( QGraphicsSceneMouseEvent * e ) {
	click_pos=e->pos();
	for (int i=0;i<ref.size();i++) {
		if (ref.at(i)->rect().contains(mapToItem(ref.at(i), e->pos()))) {
			moveRef.append(i);
		}
	}
	if (moveRef.size()>0) { // if more that one just pick the las
		int keeplast=moveRef.last();
		moveRef.clear();
		moveRef.append(keeplast);
		showMessage(tr("Moving node ")+QString::number(keeplast+1));
	} else { // if none is selected, append ref.size() to move the whole objec
		moveRef.append(ref.size());
		showMessage(tr("Moving object"));
	}

	QGraphicsItem::mousePressEvent(e);

}

void nLine::mouseReleaseEvent ( QGraphicsSceneMouseEvent * e ) {
	moveRef.clear();
	QGraphicsItem::mouseReleaseEvent(e);
	itemChanged();
}

void nLine::mouseMoveEvent ( QGraphicsSceneMouseEvent * e ) {
	nodeSelected=-1;
	if (moveRef.contains(ref.size())) {
		QPointF delta=e->pos()-click_pos;
	    moveBy(delta);
	}
    click_pos=e->pos();
	QGraphicsItem::mouseMoveEvent(e);
}

void nLine::togglePadella() {
	if (my_pad.isHidden()) {
		my_pad.show();
		updatePlot();
	} else {
		my_pad.hide();
	}
}

void nLine::mouseDoubleClickEvent ( QGraphicsSceneMouseEvent * e ) {
	togglePadella();
	QGraphicsItem::mouseDoubleClickEvent(e);
}

void nLine::export_txt(){
	// exports poly to file
	QVariant varia=property("txtFile");
	QString fname;
	if (varia.isValid()) {
		fname=varia.toString();
	} else {
		fname=QString("lineData.txt");
	}

	QString fnametmp=QFileDialog::getSaveFileName(&my_pad,tr("Save data in text"),fname,tr("Text files (*.txt *.csv);;Any files (*)"));

	if (!fnametmp.isEmpty()) {
		setProperty("txtFile",fnametmp);
		QFile t(fnametmp);
		t.open(QIODevice::WriteOnly| QIODevice::Text);
		QTextStream out(&t);
		out << getStringData(poly(numPoints));
		t.close();
		showMessage(tr("Data saved in file ")+fnametmp);
	}
}

void
nLine::copyPoints() {
	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setText(getStringData(getPoints()));
	showMessage(tr("Points copied to clipboard"));
}

void
nLine::export_txt_points() {
	if (!property("filePoints").isValid()) {
		setProperty("filePoints",my_pad.windowTitle()+QString(".txt"));
	}
	QString fnametmp=QFileDialog::getSaveFileName(&my_pad,tr("Save data"),property("filePoints").toString(),tr("Text files (*.txt *.csv);;Any files (*)"));
	if (!fnametmp.isEmpty()) {
		setProperty("filePoints",fnametmp);
		QFile t(fnametmp);
		t.open(QIODevice::WriteOnly| QIODevice::Text);
		QTextStream out(&t);
		for (int i=0; i<my_w.points->rowCount(); i++) {
			for (int j=0; j<my_w.points->columnCount();j++) {
				out << my_w.points->item(i, j)->text().toDouble() << "\t";
			}
			out << "\n";
		}
		t.close();
		showMessage(tr("Export in file:")+fnametmp);
	} else {
		showMessage(tr("Export canceled"));
	}
}

QString nLine::getStringData(QPolygonF vals){
	QString point_table;
	double dist=0.0;
	for (int i=0; i<vals.size(); i++) {
		if (i>0) 
			dist+=sqrt(pow(vals.at(i).x()-vals.at(i-1).x(),2)+pow(vals.at(i).y()-vals.at(i-1).y(),2));
		point_table.append(QString("%1\t%2\t%3").arg(dist).arg(vals.at(i).x()).arg(vals.at(i).y()));
		if (parent()->currentBuffer) {
			vec2f orig = parent()->currentBuffer->get_origin();
			point_table.append(QString("\t%1\n").arg(parent()->currentBuffer->point(vals.at(i).x()+orig.x(),vals.at(i).y()+orig.y())));
			//.alex. translate to origin
			// point_table.append(QString("\t%1\n").arg(parent()->currentBuffer->point(vals.at(i).x(),vals.at(i).y())));
		} else {
			point_table.append(QString("\n"));
		}
	}
	return point_table;
}

void nLine::copy_clip()
{
	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setText(getStringData(poly(numPoints)));
	showMessage(tr("Points copied to clipboard"));
}

void nLine::updatePlot () {
    if (my_w.plot->isVisible() && parent()->currentBuffer) {

        if (my_w.plot->graphCount()==0) {
            my_w.plot->addGraph(my_w.plot->xAxis, my_w.plot->yAxis);
            my_w.plot->graph(0)->setPen(QPen(Qt::blue));
            my_w.plot->xAxis->setLabel(tr("distance"));
            my_w.plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
            my_w.plot->xAxis->setLabelPadding(-1);
            my_w.plot->yAxis->setLabelPadding(-1);
        }

		double colore;
        QVector<double> toPlotx;
        QVector<double> toPloty;

		nPhysD *mat=parent()->currentBuffer;

		QPolygonF my_points;
		foreach(QGraphicsEllipseItem *item, ref){
			my_points<<item->pos();
		}



		QPolygonF my_poly=poly(numPoints);

        my_w.plot->clearItems();

		QPen penna;
		penna.setColor(ref[0]->brush().color());

		vec2f orig = mat->get_origin();
		double dist=0.0;
		for(int i=0;i<my_poly.size()-1;i++) {
			QPointF p=my_poly.at(i);
			if (antialias) {
				// points in poly are NOT translated with origin
				// (hence a correction must apply)
				colore=mat->getPoint(p.x()+orig.x(),p.y()+orig.y());
			} else {
				colore=mat->point((int)(p.x()+orig.x()),(int)(p.y()+orig.y()));
			}
            if (std::isfinite(colore)) {
                toPlotx << dist;
                toPloty << colore;
            }
			if (my_points.contains(p) && nSizeHolder>0.0) {
                QCPItemLine *marker=new QCPItemLine(my_w.plot);
                marker->start->setCoords(dist, QCPRange::minRange);
                marker->end->setCoords(dist, QCPRange::maxRange);
                marker->setPen(penna);
                my_w.plot->addItem(marker);
			}
			dist+=sqrt(pow((my_poly.at(i+1)-my_poly.at(i)).x(),2)+pow((my_poly.at(i+1)-my_poly.at(i)).y(),2));
		}
		if (antialias) {
			colore=mat->getPoint(my_poly.last().x()+orig.x(),my_poly.last().y()+orig.y());
		} else {
			colore=mat->point((int)(my_poly.last().x()+orig.x()),(int)(my_poly.last().y()+orig.y()));
		}
        if (std::isfinite(colore)) {
            toPlotx << dist;
            toPloty << colore;
		}

        my_w.plot->xAxis->setTickLabelFont(parent()->my_w.my_view->font());
        my_w.plot->yAxis->setTickLabelFont(parent()->my_w.my_view->font());

        my_w.plot->graph(0)->setData(toPlotx,toPloty);
        my_w.plot->rescaleAxes();
        my_w.plot->replot();
	}

}

void nLine::toggleBezier () {
	toggleBezier(!bezier);
}

void nLine::toggleBezier ( bool val ) {
	prepareGeometryChange();
	bezier=val;
	if (val) {
		showMessage(tr("Line is Bezier curve"));
	} else {
		showMessage(tr("Line is a polygonal chain"));
	}
	updatePlot();
    itemChanged();
}

void nLine::toggleClosedLine () {
	toggleClosedLine(!closedLine);
}

void nLine::toggleClosedLine ( bool val ) {
	prepareGeometryChange();
	closedLine=val;
	if (val) {
		showMessage(tr("Line is closed"));
	} else {
		showMessage(tr("Line is open"));
	}
	updatePlot();
    itemChanged();
}

void nLine::toggleAntialias () {
	toggleAntialias(!antialias);
}

void nLine::toggleAntialias ( bool val ) {
	prepareGeometryChange();
	antialias=val;
	if (val) {
		showMessage(tr("Line is antialiased"));
	} else {
		showMessage(tr("Line is antialiased"));
	}
	updatePlot();
}

void nLine::sizeHolder ( double val ) {
	nSizeHolder=val;
	QPointF p=QPointF(val/zoom,val/zoom);
	foreach(QGraphicsEllipseItem *item, ref){
		item->setRect(QRectF(-p,p));
	}
}

void nLine::setNumPoints ( int val ) {
	numPoints=val;
}

void
nLine::movePoints (QPointF p) {
	for (int i=0;i<ref.size(); i++) {
		if (moveRef.contains(i)) {
			changeP(i,p);
		}
	}
}

void
nLine::changeToolTip (QString n) {
	setToolTip(n);
	my_pad.setWindowTitle(n);
}

void
nLine::setWidthF (double w) {
	nWidth=w;
	update();
}

void
nLine::setOrder (double w) {
	setZValue(w);
}



void
nLine::tableUpdated (QTableWidgetItem * item) {
	QPointF p;
	p.rx()=my_w.points->item(item->row(),0)->text().toDouble();
	p.ry()=my_w.points->item(item->row(),1)->text().toDouble();
	changeP(item->row(),p, true);
}

void
nLine::changeColor () {
	QColorDialog colordial(my_w.colorLabel->palette().color(QPalette::Background));
	colordial.setOption(QColorDialog::ShowAlphaChannel);
	colordial.exec();
	if (colordial.result() && colordial.currentColor().isValid()) {
		changeColor(colordial.currentColor());
	}
	update();
}

void
nLine::changeColor (QColor col) {
	colorLine=col;
	my_w.colorLabel->setPalette(QPalette(colorLine));
}

void
nLine::changeColorHolder () {
	QColor color;
	QColorDialog colordial(my_w.colorHolderLabel->palette().color(QPalette::Background));
	colordial.setOption(QColorDialog::ShowAlphaChannel);
	colordial.exec();
	if (colordial.result() && colordial.currentColor().isValid()) {
		changeColorHolder(colordial.currentColor());
	}
}

void
nLine::changeColorHolder (QColor color) {
	my_w.colorHolderLabel->setPalette(QPalette(color));
	if (ref.size()>0) {
		QBrush brush=ref[0]->brush();
		brush.setColor(color);
		foreach (QGraphicsEllipseItem *item, ref){
			item->setBrush(brush);
		}
	}
}

void
nLine::changeP (int np, QPointF p, bool isLocal) {
	// if the point is already in matrix coordinates (isLocal=true) (eg. from QTableWid) changeP must not apply transformation
	//
	prepareGeometryChange();
	if (isLocal) {
		ref[np]->setPos(p);
	} else {
		ref[np]->setPos(mapFromScene(p));
	}
	ref[np]->setVisible(true);
	changePointPad(np);
	updatePlot();
}

void nLine::changePointPad(int nrow) {
	disconnect(my_w.points, SIGNAL(itemChanged(QTableWidgetItem * )), this, SLOT(tableUpdated(QTableWidgetItem * )));
	QPointF p=ref[nrow]->pos();
	QTableWidgetItem *xitem= new QTableWidgetItem(QString::number(p.x()));
	QTableWidgetItem *yitem= new QTableWidgetItem(QString::number(p.y()));
	xitem->setTextAlignment(Qt::AlignHCenter + Qt::AlignVCenter);
	yitem->setTextAlignment(Qt::AlignHCenter + Qt::AlignVCenter);
	my_w.points->setItem(nrow, 0, xitem);
	my_w.points->setItem(nrow, 1, yitem);
	my_w.points->resizeRowToContents(nrow);
	connect(my_w.points, SIGNAL(itemChanged(QTableWidgetItem * )), this, SLOT(tableUpdated(QTableWidgetItem * )));
}

void nLine::addPoint () {
	int i=ref.size();
	if (my_w.points->selectedRanges().size()>0) {
		i=my_w.points->selectedRanges().first().topRow();
	}
	moveRef.clear();
	addPoint(i);
	moveRef.removeLast();
	showMessage(tr("Added point:")+QString::number(i+1));
}

void nLine::addPoint (int npos) {
    moveRef.append(npos);
	QPointF position;
	QBrush refBrush;
	QPen refPen;
	if (ref.size()>0) {
        int copyfrom=std::max(1,std::min(ref.size()-1,npos));
		position=ref[copyfrom-1]->pos();
		refBrush=ref[copyfrom-1]->brush();
		refPen=ref[copyfrom-1]->pen();
	} else {
		position=QPointF(0,0);
		refPen.setStyle(Qt::NoPen);
		refBrush.setStyle(Qt::SolidPattern);
		refBrush.setColor(colorHolder);
	}

	ref.insert(npos,new QGraphicsEllipseItem());
	ref[npos]->setPos(position);
	ref[npos]->setBrush(refBrush);
	ref[npos]->setPen(refPen);
	ref[npos]->setVisible(false);
	ref[npos]->setParentItem(this);
	sizeHolder(nSizeHolder);
	setNumPoints(numPoints);

	disconnect(my_w.points, SIGNAL(itemChanged(QTableWidgetItem * )), this, SLOT(tableUpdated(QTableWidgetItem * )));
	my_w.points->insertRow(npos);
	QTableWidgetItem *xitem= new QTableWidgetItem(QString::number(position.x()));
	QTableWidgetItem *yitem= new QTableWidgetItem(QString::number(position.y()));
	xitem->setTextAlignment(Qt::AlignHCenter + Qt::AlignVCenter);
	yitem->setTextAlignment(Qt::AlignHCenter + Qt::AlignVCenter);
	my_w.points->setItem(npos, 0, xitem);
	my_w.points->setItem(npos, 1, yitem);
	my_w.points->resizeRowToContents(npos);
	connect(my_w.points, SIGNAL(itemChanged(QTableWidgetItem * )), this, SLOT(tableUpdated(QTableWidgetItem * )));
}

void nLine::appendPoint () {
	addPoint(ref.size());
}


void nLine::removeLastPoint() {
	removePoint(ref.size()-1);
}

void nLine::removePoint(int np) {
	if (property("parentPanControlLevel").toInt()<2) {
		if (ref.size()==2) {
			showMessage(tr("Can't remove ")+toolTip());
		} else {
			if (np>=0 && np<ref.size()) {
				prepareGeometryChange();
				delete ref.at(np);
				ref.removeAt(np);
				my_w.points->removeRow(np);
				update();
			}
			if (ref.size()==1) {
				deleteLater();
			}
            itemChanged();
		}
	}
    nodeSelected=-1;
}

void
nLine::removePoint() {
	QString removedrows;
	foreach (QTableWidgetSelectionRange r, my_w.points->selectedRanges()) {
		for (int i=r.topRow(); i <=r.bottomRow(); i++) {
			removePoint(i);
			removedrows+=QString(" ")+QString::number(i+1);
		}
	}
	showMessage(tr("Removed Rows:")+removedrows);
}

void
nLine::showMessage (QString s) {
	parent()->statusBar()->showMessage(s,2000);
	my_pad.statusBar()->showMessage(s,2000);
}

void
nLine::keyPressEvent ( QKeyEvent * e ) {
	int delta=1.0;
	if (e->modifiers() & Qt::ShiftModifier) {
		delta =10.0;
	}
    switch (e->key()) {
        case Qt::Key_W: 
            togglePadella();
            break;            
		case Qt::Key_Return:
		case Qt::Key_Escape:
			if (disconnect(parent()->my_w.my_view, SIGNAL(mouseReleaseEvent_sig(QPointF)), this, SLOT(addPointAfterClick(QPointF)))) {
				removeLastPoint();
				showMessage(tr("Adding points ended"));
                ungrabKeyboard();
			}
            moveRef.clear();
			break;
		case Qt::Key_Up:
			moveBy(QPointF(0.0,-delta));
			itemChanged();
			break;
		case Qt::Key_Down:
			moveBy(QPointF(0.0,+delta));
			itemChanged();
			break;
		case Qt::Key_Left:
			moveBy(QPointF(-delta,0.0));
			itemChanged();
			break;
		case Qt::Key_Right:
			moveBy(QPointF(+delta,0.0));
			itemChanged();
			break;
		case Qt::Key_B:
			toggleBezier();
			updatePlot();
			break;
		case Qt::Key_S:
			toggleAntialias();
			updatePlot();
			break;
		case Qt::Key_C:
            if (e->modifiers() & Qt::ShiftModifier) {
                centerOnImage();
            } else {
                toggleClosedLine();
                updatePlot();
            }
			break;
		case Qt::Key_A:
            contextAppendPoint();
			break;
		case Qt::Key_P:
            contextPrependPoint();
			break;
		case Qt::Key_D:
            contextRemovePoint();
			break;
		case Qt::Key_X: {
            makeHorizontal();
			break;
		}
		case Qt::Key_Y:{
            makeVertical();
			break;
		}
		case Qt::Key_R:{
            makeRectangle();
			break;
		}
		default:
            emit key_pressed(e->key());
            break;
	}
    e->accept();
}


void nLine::makeHorizontal() {
    if (parent()->currentBuffer) {
        QPointF p(0,0);
        foreach (QGraphicsEllipseItem *item, ref){
            p-=item->pos();
        }
        p/=ref.size();
        vec2f p1=parent()->currentBuffer->getSize()-parent()->currentBuffer->get_origin();
        p+=QPointF(p1.x(),p1.y());

        while (ref.size()>2) removePoint(2);

        changeP(0, QPointF(0.0,p.y()));
        changeP(1, QPointF(parent()->currentBuffer->getW(),p.y()));
        

        itemChanged();
        moveRef.clear();
    } 
}

void nLine::makeVertical() {
    if (parent()->currentBuffer) {
        QPointF p(0,0);
        foreach (QGraphicsEllipseItem *item, ref){
            p-=item->pos();
        }
        p/=ref.size();
        vec2f p1=parent()->currentBuffer->getSize()-parent()->currentBuffer->get_origin();
        p+=QPointF(p1.x(),p1.y());
        
        while (ref.size()>2) removePoint(2);

        changeP(0, QPointF(p.x(),0.0));
        changeP(1, QPointF(p.x(),parent()->currentBuffer->getH()));
        
        itemChanged();
    }
}

void nLine::makeRectangle() {
    QRectF my_rect=path().boundingRect();

    while (ref.size()<4) appendPoint();
    while (ref.size()>4) removePoint(4);
    
    QPointF p1=my_rect.topLeft();
    QPointF p2=my_rect.bottomRight();
    
    changeP(0, p1);
    changeP(1, QPointF(p1.x(),p2.y()));
    changeP(2, p2);
    changeP(3, QPointF(p2.x(),p1.y()));
    
}

void
nLine::keyReleaseEvent ( QKeyEvent *  ) {
}


void
nLine::moveBy(QPointF delta) {
    if (property("parentPanControlLevel").toInt()<2) {
		for (int i =0; i<ref.size(); i++) {
			changeP(i,mapToScene(ref[i]->pos()+delta));
		}
	}
}

void
nLine::hoverEnterEvent( QGraphicsSceneHoverEvent *e) {
	setFocus(Qt::MouseFocusReason);
	for (int i=0;i<ref.size();i++) {
        QRectF my_rect=ref.at(i)->rect();
		if (my_rect.contains(mapToItem(ref.at(i), e->pos()))) {
			nodeSelected=i;
            showMessage(toolTip()+":"+QString::number(i+1));
            break;
		}
	}
}

void
nLine::hoverLeaveEvent( QGraphicsSceneHoverEvent *) {
	clearFocus();
}

void
nLine::hoverMoveEvent( QGraphicsSceneHoverEvent *e) {
	for (int i=0;i<ref.size();i++) {
        QRectF my_rect=ref.at(i)->rect();
		if (my_rect.contains(mapToItem(ref.at(i), e->pos()))) {
			nodeSelected=i;
            showMessage(toolTip()+":"+QString::number(i+1));
            break;
		}
	}
}

void nLine::contextMenuEvent ( QGraphicsSceneContextMenuEvent * e ) {
    QMenu menu;
    if (nodeSelected>=0) {
        moveRef.clear();
        QAction *append = menu.addAction("Append point "+QString::number(nodeSelected+1)+" (a)");
        connect(append, SIGNAL(triggered()), this, SLOT(contextAppendPoint()));
        QAction *prepend = menu.addAction("Prepend point "+QString::number(nodeSelected+1)+" (p)");
        connect(prepend, SIGNAL(triggered()), this, SLOT(contextPrependPoint()));
        QAction *remove = menu.addAction("Delete point "+QString::number(nodeSelected+1)+" (d)");
        connect(remove, SIGNAL(triggered()), this, SLOT(contextRemovePoint()));
        menu.addAction(menu.addSeparator());
    }
    QAction *bezier = menu.addAction("Straight/Bezier (b)");
    connect(bezier, SIGNAL(triggered()), this, SLOT(toggleBezier()));
    QAction *closedline = menu.addAction("Closed/Open (c)");
    connect(closedline, SIGNAL(triggered()), this, SLOT(toggleClosedLine()));
    QAction *centerimage = menu.addAction("Center on image (C)");
    connect(centerimage, SIGNAL(triggered()), this, SLOT(centerOnImage()));
    QAction *twoHoriz = menu.addAction("2 points Horizontal (x)");
    connect(twoHoriz, SIGNAL(triggered()), this, SLOT(makeHorizontal()));
    QAction *twoVert = menu.addAction("2 points Vertical (y)");
    connect(twoVert, SIGNAL(triggered()), this, SLOT(makeVertical()));
    QAction *makeRect = menu.addAction("4 points Rectangle (r)");
    connect(makeRect, SIGNAL(triggered()), this, SLOT(makeRectangle()));
    menu.addAction(menu.addSeparator());
    QAction *showPan = menu.addAction("Show control (w)");
    connect(showPan, SIGNAL(triggered()), this, SLOT(togglePadella()));
    menu.exec(e->screenPos());
}


void nLine::contextAppendPoint(){
    addPoint(nodeSelected+1);
    showMessage(tr("Append to point ")+QString::number(nodeSelected));
}

void nLine::contextPrependPoint(){
    addPoint(nodeSelected);
    showMessage(tr("Prepend to point ")+QString::number(nodeSelected));
}

void nLine::contextRemovePoint(){
    removePoint(nodeSelected);
    showMessage(tr("Delete point ")+QString::number(nodeSelected));
}

void
nLine::focusInEvent( QFocusEvent *) {
	selectThis(true);
}

void
nLine::focusOutEvent( QFocusEvent *) {
    selectThis(false);
}

void
nLine::selectThis(bool val) {
	setSelected(val);
	for (int i =0; i<ref.size(); i++) {
		ref[i]->setVisible(val);
	}
	if (val) {
        grabKeyboard();
        parent()->my_w.statusbar->showMessage(toolTip());
    } else {
        ungrabKeyboard();
        parent()->my_w.statusbar->clearMessage();
    }
}

void nLine::itemChanged() {
	emit sceneChanged();
}

// reimplementation
QRectF
nLine::boundingRect() const {
    QPainterPath my_path=shape();
	return my_path.boundingRect();
}

QPainterPath nLine::shape() const {
	QPainterPathStroker stroker;
    double thickness=std::max(nWidth,10.0)/zoom;
    stroker.setWidth(thickness);
	QPainterPath my_shape = stroker.createStroke( path() );
	for (int i =0; i<ref.size(); i++) {
        QRectF my_rect=ref[i]->rect();
		my_shape.addPolygon(ref[i]->mapToScene(my_rect));
	}
	return my_shape;
}

void
nLine::paint(QPainter* p, const QStyleOptionGraphicsItem* , QWidget* ) {
	//	p->setCompositionMode((QPainter::CompositionMode)22);
	QPen pen;
	pen.setWidthF(nWidth/zoom);
	pen.setColor(colorLine);
	p->setPen(pen);
	p->drawPath(path());
}

QPainterPath nLine::path() const {
	QPainterPath my_path;
	my_path.addPolygon(poly(1));
	return my_path;
}

QPolygonF nLine::poly(int steps) const {
	QPolygonF my_poly, my_poly_interp;
	
	if(ref.size()>0) {
		foreach (QGraphicsEllipseItem *item, ref){
			my_poly<< item->pos();
		}

        if (bezier && my_poly.size()>2) {

            std::vector<double> T,X,Y;
            if (closedLine)  {
                T.resize(my_poly.size()+4);
                X.resize(my_poly.size()+4);
                Y.resize(my_poly.size()+4);
                for (unsigned int i = 0; i < T.size(); i++ ) {
                    T[i]=i-2;
                    X[i]=my_poly[(i+ref.size()-2)%ref.size()].x();
                    Y[i]=my_poly[(i+ref.size()-2)%ref.size()].y();
                }
            } else {
                T.resize(my_poly.size());
                X.resize(my_poly.size());
                Y.resize(my_poly.size());
                for (int i = 0; i < my_poly.size(); i++ ) {
                    T[i]=i;
                    X[i]=my_poly[i].x();
                    Y[i]=my_poly[i].y();
                }
            }

            tk::spline sX, sY;

            sX.set_points(T,X);
            sY.set_points(T,Y);

            steps=std::max(steps,32); // if it's a bezier impose at least 16 steps...
            int size=steps*(my_poly.size()-(closedLine?0:1));
            
            for (int i = 0; i < size+1; i++ ) {
                const double dtmp = (double) i / steps;
                my_poly_interp.append(QPointF(sX(dtmp),sY(dtmp)));
            }
            
        } else {
            if (closedLine) my_poly << ref[0]->pos();
            for(int i=0;i<my_poly.size()-1;i++) {
                QPointF p1=my_poly.at(i);
                QPointF p2=my_poly.at(i+1);
                for(int j=0;j<=steps;j++) {
                    my_poly_interp << p1+j*(p2-p1)/steps;
                }
            }
        }
    }

    return my_poly_interp;
}

void nLine::centerOnImage(){
    
    if (parent()->currentBuffer) {
        QPointF p(0,0);
        foreach (QGraphicsEllipseItem *item, ref){
            p-=item->pos();
        }
        p/=ref.size();
        vec2f p1=parent()->currentBuffer->getSize()/2.0-parent()->currentBuffer->get_origin();
        p+=QPointF(p1.x(),p1.y());
        moveBy(p);
    }
    
}

void
nLine::rearrange_monotone() {
	// ordering = true : decides monotonicity to minimize the transverse distance. ordering = false: opposite
	bool horizontal;
	QPolygonF my_poly;
	foreach (QGraphicsEllipseItem *item, ref){
		my_poly<< item->pos();
	}

    if (std::abs(my_poly.last().rx()-my_poly.first().rx()) > std::abs(my_poly.last().ry()-my_poly.first().ry()))
		horizontal = true;
	else
		horizontal = false;

	if (forceInverseOrdering)
		horizontal = !horizontal;

	if (horizontal) {
		qSort(my_poly.begin(),my_poly.end(), orderMonotone_x);
		parent()->statusBar()->showMessage("Axis is HORIZONTAL");
		//std::cerr<<"[nLine] axis is horizontal"<<std::endl;
	} else {
		qSort(my_poly.begin(),my_poly.end(), orderMonotone_y);
		parent()->statusBar()->showMessage("Axis is VERTICAL");
		//std::cerr<<"[nLine] axis is vertical"<<std::endl;
	}

	int ii=0;
	prepareGeometryChange();
	foreach (QGraphicsEllipseItem *item, ref){
		item->setPos(my_poly[ii]);
		ii++;
	}


}

bool
nLine::getHMonotone()
{
	bool horizontal;
	QPolygonF my_poly;
	foreach (QGraphicsEllipseItem *item, ref){
		my_poly<< item->pos();
	}

    if (std::abs(my_poly.last().rx()-my_poly.first().rx()) > std::abs(my_poly.last().ry()-my_poly.first().ry()))
		horizontal = true;
	else
		horizontal = false;

	if (forceInverseOrdering)
		horizontal = !horizontal;

	return horizontal;
}

void
nLine::setMonotone(bool sm)
{ forceMonotone = sm; }

void //TODO: non credo che forceInverseOrdering serva
nLine::switchOrdering()
{ forceInverseOrdering = !forceInverseOrdering; rearrange_monotone(); }

void nLine::zoomChanged(double val){
	zoom=val;
	sizeHolder(nSizeHolder);
	setWidthF(nWidth);
	update();
}



// ------------------------ static ---------------------------
bool orderMonotone_x(const QPointF &p1, const QPointF &p2)
{ return ( p1.x() < p2.x() ); }

bool orderMonotone_y(const QPointF &p1, const QPointF &p2)
{ return ( p1.y() < p2.y() ); }


// SETTINGS

void
nLine::loadSettings() {
	if (!property("fileIni").isValid()) {
		setProperty("fileIni",QString("line.ini"));
	}
	QString fnametmp = QFileDialog::getOpenFileName(&my_pad, tr("Open INI File"),property("fileIni").toString(), tr("INI Files (*.ini *.conf)"));
	if (!fnametmp.isEmpty()) {
		setProperty("fileIni",fnametmp);
		QSettings settings(fnametmp,QSettings::IniFormat);
		loadSettings(&settings);
	}
}

void
nLine::saveSettings() {
	if (!property("fileIni").isValid()) {
		setProperty("fileIni",QString("line.ini"));
	}
	QString fnametmp = QFileDialog::getSaveFileName(&my_pad, tr("Save INI File"),property("fileIni").toString(), tr("INI Files (*.ini *.conf)"));
	if (!fnametmp.isEmpty()) {
		setProperty("fileIni",fnametmp);
		QSettings settings(fnametmp,QSettings::IniFormat);
		settings.clear();
		saveSettings(&settings);
	}
}

void
nLine::loadSettings(QSettings *settings) {
	settings->beginGroup(toolTip());
    setPos(settings->value("position").toPoint());

	if (property("parentPanControlLevel").toInt()<2) {
		int size = settings->beginReadArray("points");
		QPolygonF poly_tmp;
		for (int i = 0; i < size; ++i) {
			settings->setArrayIndex(i);
            QPointF ppos=QPointF(settings->value("x").toDouble(),settings->value("y").toDouble());
            poly_tmp << ppos;
		}
		settings->endArray();
		if (poly_tmp.size()>0) {
			setPoints(poly_tmp);
		} else {
			showMessage(tr("Error reading from file"));
		}
	}
	setToolTip(settings->value("name",toolTip()).toString());
	setZValue(settings->value("depth",zValue()).toDouble());
	setWidthF(settings->value("width",nWidth).toDouble());
	changeColor(settings->value("colorLine",colorLine).value<QColor>());
	toggleBezier(settings->value("bezier",bezier).toBool());
	toggleClosedLine(settings->value("closedLine",closedLine).toBool());
	toggleAntialias(settings->value("antialias",antialias).toBool());
	sizeHolder(settings->value("sizeHolder",nSizeHolder).toDouble());
	changeColorHolder(settings->value("colorHolder",colorHolder).value<QColor>());
	setNumPoints(settings->value("samplePoints",numPoints).toInt());
	settings->endGroup();
}

void
nLine::saveSettings(QSettings *settings) {
	settings->beginGroup(toolTip());
	settings->remove("");
    settings->setValue("position",pos());	
	if (property("parentPanControlLevel").toInt()<2) {
		settings->beginWriteArray("points");
		for (int i = 0; i < ref.size(); ++i) {
			settings->setArrayIndex(i);
            QPointF ppos=mapToScene(ref.at(i)->pos());
            settings->setValue("x", ppos.x());
            settings->setValue("y", ppos.y());
		}
		settings->endArray();
	}
	settings->setValue("name",toolTip());
	settings->setValue("depth",zValue());
	settings->setValue("width",nWidth);
	settings->setValue("colorLine",colorLine);
	settings->setValue("bezier",bezier);
	settings->setValue("closedLine",closedLine);
	settings->setValue("antialias",antialias);
	settings->setValue("sizeHolder",nSizeHolder);
	if (ref.size()>0)	settings->setValue("colorHolder",ref[0]->brush().color());
	settings->setValue("samplePoints",numPoints);
	settings->endGroup();
}
