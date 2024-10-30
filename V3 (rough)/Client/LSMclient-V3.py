from PyQt5.QtWidgets import *
from PyQt5 import QtGui, QtCore, uic
from PyQt5.QtGui import QIntValidator
import serial.tools.list_ports
import serial
import numpy as np
import pyqtgraph as pg
from time import sleep

ports = serial.tools.list_ports.comports()
device = serial.Serial()
device.baudrate = 115200 # if bad values use 9600

lsm_resolution = 65536



class ui(QMainWindow):
    def __init__(self):
        super(ui, self).__init__() # Call the inherited classes __init__ method
        uic.loadUi('LSM-client.ui', self) # Load the .ui file
        self.show() # Show the GUI

        #add com ports
        for port in ports:
            action = QAction(port.name, self)
            action.triggered.connect(self.setPort)
            self.menuDevice.addAction(action)

        #vars
        self.xRes = 200
        self.yRes = 200
        self.xOffset = int((lsm_resolution/2) - 1)
        self.yOffset = int((lsm_resolution/2) - 1)
        self.skipSteps = 80
        self.laserIntensity = 2200
        self.adcGain = 0
        self.adcAvg = 4
        self.scanned = False

        #define actions
        self.graph_widget.hide()
        self.scan_button.clicked.connect(self.scan)
        self.coarse_button.clicked.connect(self.coarseFocus)
        self.fine_button.clicked.connect(self.fineFocus)
        self.save_button.clicked.connect(self.save)
        self.x_resolution_edit.textChanged.connect(self.setxRes)
        self.y_resolution_edit.textChanged.connect(self.setyRes)
        self.x_offset_slider.setRange(0, lsm_resolution-2)
        self.y_offset_slider.setRange(0, lsm_resolution-2)
        self.x_offset_slider.valueChanged.connect(self.xSlider)
        self.y_offset_slider.valueChanged.connect(self.ySlider)
        self.zoom_slider.setRange(1, 99)
        self.zoom_slider.valueChanged.connect(self.zoom)
        self.laser_slider.setRange(1500, 2500)
        self.laser_slider.valueChanged.connect(self.laser)
        self.gain_combo.currentIndexChanged.connect(self.gain)
        self.avg_combo.currentIndexChanged.connect(self.avg)

        onlyInt = QIntValidator()
        onlyInt.setRange(1, lsm_resolution-1)
        self.x_resolution_edit.setValidator(onlyInt)
        self.y_resolution_edit.setValidator(onlyInt)

        #set defaults
        self.x_resolution_edit.setText(str(self.xRes))
        self.y_resolution_edit.setText(str(self.yRes))
        self.x_offset_slider.setValue(self.xOffset)
        self.y_offset_slider.setValue(self.yOffset)
        self.zoom_slider.setValue(self.skipSteps)
        self.laser_slider.setValue(self.laserIntensity)
        self.avg_combo.setCurrentIndex(self.adcAvg)


    #actions
    def scan(self):

        #do math here to get the max skipSteps, and map the zoom 0-100 range to 1 to max skipSteps
        #Calculate maxSkipSteps from x and y seperately, then compare and take the lower value as the max.

        #X
        if self.xOffset >= (lsm_resolution/2)-1:
            xMinWidth = (lsm_resolution - 1) - self.xOffset
        else:
            xMinWidth = self.xOffset
            
        xMaxSkipSteps = int(xMinWidth/(self.xRes/2)) #only half of xRes on each side of the offset point
        localXSkipSteps = int(1+(((99-self.skipSteps) / 98) * (xMaxSkipSteps-1)))

        #Y
        if self.yOffset >= (lsm_resolution/2)-1:
            yMinWidth = (lsm_resolution - 1) - self.yOffset
        else:
            yMinWidth = self.yOffset
        yMaxSkipSteps = int(yMinWidth/(self.yRes/2))
        localYSkipSteps = int(1+(((99-self.skipSteps) / 98) * (yMaxSkipSteps-1)))

        if localXSkipSteps >= localYSkipSteps:
            localSkipSteps = localYSkipSteps
        else:
            localSkipSteps = localXSkipSteps

        localxRes = self.xRes
        localyRes = self.yRes
        #disable buttons
        self.scan_button.setEnabled(False)
        self.coarse_button.setEnabled(False)
        self.fine_button.setEnabled(False)
        self.save_button.setEnabled(False)
        
        self.graph_widget.hide()
        self.image_view.show()
        image = np.zeros((localyRes, localxRes), dtype=int)
        if device.is_open:
            #clear buffer
            device.reset_input_buffer()
            device.reset_output_buffer()

            #set values
            device.write(bytes('1' + str(localxRes), 'ascii'))
            sleep(0.1)
            device.write(bytes('2' + str(localyRes), 'ascii'))
            sleep(0.1)
            device.write(bytes('4' + str(self.xOffset), 'ascii'))
            sleep(0.1)
            device.write(bytes('5' + str(self.yOffset), 'ascii'))
            sleep(0.1)
            device.write(bytes('3' + str(localSkipSteps), 'ascii'))
            sleep(0.1)
            device.write(bytes('7' + str(self.laserIntensity), 'ascii'))
            sleep(0.1)
            device.write(bytes('6' + str(self.adcGain), 'ascii'))
            sleep(0.1)
            device.write(bytes('8' + str(self.adcAvg), 'ascii'))
            sleep(0.1)

            progress = QProgressDialog("Scan progress: ", None, 0, localyRes)
            progress.setWindowModality(QtCore.Qt.WindowModal)


            

            #start scan
            device.write(bytes('01', 'ascii'))
            for y in range(int(localyRes)):
                currentLine = device.readline() # might time out
                currentLine = currentLine.decode("ascii") 
                if "Offset too small" in currentLine:
                    QMessageBox.about(self, "Error", " Scan error: Decrease zoom or resolution")
                    #enable buttons
                    self.scan_button.setEnabled(True)
                    self.coarse_button.setEnabled(True)
                    self.fine_button.setEnabled(True)
                    self.save_button.setEnabled(True)
                    #clear buffer
                    device.reset_input_buffer()
                    device.reset_output_buffer()
                    return
                elif "Dimensions too large" in currentLine:
                    QMessageBox.about(self, "Error", " Scan error: Dimensions too large")
                    #enable buttons
                    self.scan_button.setEnabled(True)
                    self.coarse_button.setEnabled(True)
                    self.fine_button.setEnabled(True)
                    self.save_button.setEnabled(True)
                    #clear buffer
                    device.reset_input_buffer()
                    device.reset_output_buffer()
                    return
                elif "Failed to send image" in currentLine:
                    QMessageBox.about(self, "Error", " Scan error: Failed to send image")
                    #enable buttons
                    self.scan_button.setEnabled(True)
                    self.coarse_button.setEnabled(True)
                    self.fine_button.setEnabled(True)
                    self.save_button.setEnabled(True)
                    #clear buffer
                    device.reset_input_buffer()
                    device.reset_output_buffer()
                    return
                else:
                    tempList = currentLine.split(',')
                    for x in range(localxRes):
                        image[y][x] = int(tempList[x])
                progress.setValue(y)
            minValue = 100000
            maxValue = -100000
            for y in range(localyRes):
                for x in range(localxRes):
                    if image[y][x] > maxValue:
                        maxValue = image[y][x]
                    elif image[y][x] < minValue:
                        minValue = image[y][x]

            for y in range(localyRes):
                for x in range(localxRes):
                    image[y][x] = ((image[y][x] - minValue) / (maxValue - minValue)) * 255 #map to greyscale

            image = image.astype(np.uint8)

            #display image
            result = QtGui.QImage(image.data, localxRes, localyRes, QtGui.QImage.Format_Indexed8)
            result.ndarray = image
            for i in range(256):
                result.setColor(i, QtGui.QColor(i, i, i).rgb())
            pixmap_image = QtGui.QPixmap.fromImage(result)
            pixmap_image = pixmap_image.scaled(400, 400, QtCore.Qt.KeepAspectRatio)
            pixmap_image = QtGui.QPixmap(pixmap_image)
            self.image_view.setPixmap(pixmap_image)
            self.image_view.show()
            self.scanned = True

            #enable buttons
            sleep(1)
            self.scan_button.setEnabled(True)
            self.coarse_button.setEnabled(True)
            self.fine_button.setEnabled(True)
            self.save_button.setEnabled(True)

            #clear buffer
            device.reset_input_buffer()
            device.reset_output_buffer()
        
    def coarseFocus(self):
        goneBack = False
        if device.is_open:
            #clear buffer
            device.reset_input_buffer()
            device.reset_output_buffer()

            self.image_view.hide()
            self.graph_widget.clear()
            #setup graph
            xG = []
            yG = []
            self.graph_widget.setBackground('w')
            pen = pg.mkPen(color=(255, 0, 0))
            pen2 = pg.mkPen(color=(0, 255, 0))
            self.graph_widget.addLegend()
            data_line = self.graph_widget.plot(xG, yG, pen=pen, name="initial")
            data_line2 = self.graph_widget.plot(xG, yG, pen=pen2, name="retrace")
            self.graph_widget.show()

            #disable buttons
            self.scan_button.setEnabled(False)
            self.coarse_button.setEnabled(False)
            self.fine_button.setEnabled(False)
            self.save_button.setEnabled(False)

            #set values
            device.write(bytes('4' + str(self.xOffset), 'ascii'))
            sleep(0.1)
            device.write(bytes('5' + str(self.yOffset), 'ascii'))
            sleep(0.1)
            device.write(bytes('7' + str(self.laserIntensity), 'ascii'))
            sleep(0.1)
            device.write(bytes('6' + str(self.adcGain), 'ascii'))
            sleep(0.1)
            device.write(bytes('8' + str(self.adcAvg), 'ascii'))
            sleep(0.1)

            device.write(bytes('92', 'ascii'))
            while True:
                value = device.readline()
                value = value.decode("ascii") 

                if "Done" not in value: #first scan
                    value = value.split(',')
                    xG.append(8192-int(value[1]))
                    yG.append(int(value[0]))
                    if goneBack == False:
                        data_line.setData(yG, xG)
                    else:
                        data_line2.setData(yG, xG)
                    QtGui.QGuiApplication.processEvents()
                else:
                    if goneBack:
                        #enable buttons
                        sleep(1)
                        self.scan_button.setEnabled(True)
                        self.coarse_button.setEnabled(True)
                        self.fine_button.setEnabled(True)
                        self.save_button.setEnabled(True)
                        #clear buffer
                        device.reset_input_buffer()
                        device.reset_output_buffer()
                        break
                    else:
                        xG = []
                        yG = []
                        goneBack = True

    def fineFocus(self):
        goneBack = False
        if device.is_open:
            #clear buffer
            device.reset_input_buffer()
            device.reset_output_buffer()
        
            self.image_view.hide()
            self.graph_widget.clear()
            #setup graph
            xG = []
            yG = []
            self.graph_widget.setBackground('w')
            pen = pg.mkPen(color=(255, 0, 0))
            pen2 = pg.mkPen(color=(0, 255, 0))
            self.graph_widget.addLegend()
            data_line = self.graph_widget.plot(xG, yG, pen=pen, name="initial")
            data_line2 = self.graph_widget.plot(xG, yG, pen=pen2, name="retrace")
            self.graph_widget.show()

            #disable buttons
            self.scan_button.setEnabled(False)
            self.coarse_button.setEnabled(False)
            self.fine_button.setEnabled(False)
            self.save_button.setEnabled(False)

            #set values
            device.write(bytes('4' + str(self.xOffset), 'ascii'))
            sleep(0.1)
            device.write(bytes('5' + str(self.yOffset), 'ascii'))
            sleep(0.1)
            device.write(bytes('7' + str(self.laserIntensity), 'ascii'))
            sleep(0.1)
            device.write(bytes('6' + str(self.adcGain), 'ascii'))
            sleep(0.1)
            device.write(bytes('8' + str(self.adcAvg), 'ascii'))
            sleep(0.1)

            device.write(bytes('91', 'ascii'))

            while True:
                value = device.readline()
                value = value.decode("ascii") 

                if "Done" not in value:
                    value = value.split(',')
                    xG.append(int(value[1]))
                    yG.append(int(value[0]))
                    if goneBack == False:
                        data_line.setData(yG, xG)
                    else:
                        data_line2.setData(yG, xG)
                    QtGui.QGuiApplication.processEvents()
                else:
                    if goneBack:
                        #enable buttons
                        sleep(1)
                        self.scan_button.setEnabled(True)
                        self.coarse_button.setEnabled(True)
                        self.fine_button.setEnabled(True)
                        self.save_button.setEnabled(True)
                        #clear buffer
                        device.reset_input_buffer()
                        device.reset_output_buffer()
                        break
                    else:
                        xG = []
                        yG = []
                        goneBack = True

    def setPort(self):
        device.port = self.sender().text()
        device.open()
        if device.is_open == False:
            QMessageBox.about(self, "Failed", "Couldn't connect to: " + device.port)


    def setxRes(self):
        if str(self.x_resolution_edit.text()).isnumeric():
            self.xRes = int(self.x_resolution_edit.text())

    def setyRes(self):
        if str(self.y_resolution_edit.text()).isnumeric():
            self.yRes = int(self.y_resolution_edit.text())

    def xSlider(self, value):
        self.x_offset_label.setText("X offset: " + str(value))
        self.xOffset = value
    def ySlider(self, value):
        self.y_offset_label.setText("Y offset: " + str(value))
        self.yOffset = value

    def zoom(self, value):
        self.zoom_label.setText("Zoom: " + str(value))
        self.skipSteps = value        

    def laser(self, value):
        self.laser_label.setText("Laser intensity: " + str(value))
        self.laserIntensity = value

    def gain(self, index):
        self.adcGain = index

    def avg(self):
        self.adcAvg = str(self.avg_combo.currentText())

    def save(self):
        if self.image_view.isVisible() == True and self.scanned == True:
            file, _filter = QFileDialog.getSaveFileName(self, caption='Save scan', filter='*.png')
            self.image_view.pixmap().save(file, "PNG")





app = QApplication([])
window = ui() # Create an instance of our class
app.exec_() # Start the application
