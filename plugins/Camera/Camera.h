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
#ifndef __Camera
#define __Camera

#include <QCamera>
#include <QCameraImageCapture>

#include "nGenericPan.h"
#include "ui_Camera.h"

#include <QTimer>

class neutrino;

class Camera : public nGenericPan {
    Q_OBJECT

public:	
    Q_INVOKABLE Camera(neutrino *);
    ~Camera();

    Ui::Camera my_w;
    QCamera* camera;
    QCameraImageCapture *imageCapture;

    QMenu *cameraMenu;
    nPhysD *imgGray;
    std::vector<nPhysD *> imgColor;

    QTimer timeLapse;

public slots:
    void on_grab_clicked();
    void setupCam (const QCameraInfo &cameraInfo);
    void processCapturedImage(int requestId, const QImage &img);
    void processCapturedImage(int requestId, const QString &img);
    void giveNeutrino(const QImage &img);
    void contextMenuEvent (QContextMenuEvent *);
    void changeCameraAction();
    void on_timeLapse_valueChanged(int);

};

NEUTRINO_PLUGIN(Camera,File);

#endif
