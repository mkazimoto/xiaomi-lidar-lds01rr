﻿using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO.Ports;
using System.Diagnostics;

namespace XiaomiLidarLDS01RR
{
  public partial class FormMain : Form
  {
    private const double DegreeRadian = Math.PI * 2 / 360;

    private Point Center;
    private Point DragShift;
    private Point DragInitial;
    private Point DragFinal;

    private int[] Distances = new int[360];
    private SerialPort SerialPort;

    private Task TaskLidar;

    private bool StopLidar = false;

    public FormMain()
    {
      InitializeComponent();
    }

    private void FormMain_Load(object sender, EventArgs e)
    {
      cbListSerialPort.DataSource = SerialPort.GetPortNames();
      UpdateCenter();
    }

    private void BtnConectar_Click(object sender, EventArgs e)
    {
      try
      {
        SerialPort = new SerialPort((String)cbListSerialPort.SelectedItem);
        SerialPort.BaudRate = 115200;
        SerialPort.ReadTimeout = 5000;
        SerialPort.NewLine = "\r\n";

        SerialPort.Open();

        cbListSerialPort.Enabled = false;
        btnConectar.Enabled = false;
        btnDesconectar.Enabled = true;
      }
      catch (Exception except)
      {
        MessageBox.Show(except.Message);
        return;
      }

      TaskLidar = Task.Run(() => StartLidarLoop());
    }

    /// <summary>
    /// Thread to read the Lidar data
    /// </summary>
    private void StartLidarLoop()
    {
      StopLidar = false;
      while (!StopLidar)
      {
        try
        {
          string line = SerialPort.ReadLine();
          var splitLine = line.Split(':');

          if (splitLine.Length == 2)
          {
            var angle = Convert.ToInt32(splitLine[0]);
            Distances[angle] = Convert.ToInt32(splitLine[1]);

            if (angle == 0)
              UpdateMap(); // Paint Frame
          }
        }
        catch (Exception except)
        {
          // Ignore exceptions
          Debug.WriteLine(except.Message);
        }
      }

      SerialPort.Close();
    }

    private void BtnDesconectar_Click(object sender, EventArgs e)
    {
      Desconectar();
    }

    private void Desconectar()
    {
      try
      {
        StopLidar = true;
        if (TaskLidar != null)
          TaskLidar.Wait();

        cbListSerialPort.Enabled = true;
        btnConectar.Enabled = true;
        btnDesconectar.Enabled = false;
      }
      catch (Exception except)
      {
        MessageBox.Show(except.Message);
      }
    }

    private void Timer1_Tick(object sender, EventArgs e)
    {
      if (SerialPort == null)
      {
        var listPortsNew = SerialPort.GetPortNames();
        if (listPortsNew.Length != cbListSerialPort.Items.Count)
        {
          cbListSerialPort.DataSource = listPortsNew;
        }
      }
    }

    private void pbOutput_Paint(object sender, PaintEventArgs e)
    {
      for (int angle = 0; angle < 360; angle++)
      {
        if (Distances[angle] > 0)
        {
          int x = GetPositionX(Distances[angle], angle);
          int y = GetPositionY(Distances[angle], angle);

          e.Graphics.DrawLine(Pens.Gray, Center.X, Center.Y, x, y);
          e.Graphics.DrawArc(Pens.Lime, x, y, 3, 3, 0, 360);
        }
      }

      for (int angle = 0; angle < 360; angle += 90)
      {
        e.Graphics.DrawLine(Pens.Red, Center.X, Center.Y, GetPositionX(Distances[angle], angle), GetPositionY(Distances[angle], angle));
        e.Graphics.DrawString($"{angle}º - {Distances[angle]}mm", Font, Brushes.White, GetPositionX(500, angle), GetPositionY(500, angle));
      }
    }

    private int GetPositionY(int distance, int angle)
    {
      var angleShift = (angle + trackBarRotate.Value) % 360;
      return (int)(Math.Sin(angleShift * DegreeRadian) * distance * (double)trackBarZoom.Value / 150d + Center.Y);
    }

    private int GetPositionX(int distance, int angle)
    {
      var angleShift = (angle + trackBarRotate.Value) % 360;
      return (int)(Math.Cos(angleShift * DegreeRadian) * distance * (double)trackBarZoom.Value / 150d + Center.X);
    }

    private void FormMain_FormClosing(object sender, FormClosingEventArgs e)
    {
      Desconectar();
    }

    private void pbOutput_Resize(object sender, EventArgs e)
    {
      UpdateCenter();
      UpdateMap();
    }

    private void UpdateCenter()
    {
      Center = new Point(pbOutput.Width / 2 + DragShift.X + (DragFinal.X - DragInitial.X), pbOutput.Height / 2 + DragShift.Y + (DragFinal.Y - DragInitial.Y));
    }

    private void UpdateMap()
    {
      pbOutput.Invalidate();
    }

    private void pbOutput_MouseDown(object sender, MouseEventArgs e)
    {
      if (e.Button == MouseButtons.Left)
      {
        DragInitial = new Point(e.X, e.Y);
        DragFinal = new Point(e.X, e.Y);
        UpdateCenter();
        UpdateMap();
      }
    }

    private void pbOutput_MouseMove(object sender, MouseEventArgs e)
    {
      if (e.Button == MouseButtons.Left)
      {
        DragFinal = new Point(e.X, e.Y);
        UpdateCenter();
        UpdateMap();
      }
    }

    private void pbOutput_MouseUp(object sender, MouseEventArgs e)
    {
      if (e.Button == MouseButtons.Left)
      {
        DragShift = new Point(DragShift.X + (DragFinal.X - DragInitial.X), DragShift.Y + (DragFinal.Y - DragInitial.Y));
        DragInitial = Point.Empty;
        DragFinal = Point.Empty;
        UpdateCenter();
        UpdateMap();
      }
    }
  }
}


