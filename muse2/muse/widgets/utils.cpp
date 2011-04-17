//=========================================================
//  MusE
//  Linux Music Editor
//    $Id: utils.cpp,v 1.1.1.1.2.3 2009/11/14 03:37:48 terminator356 Exp $
//  (C) Copyright 1999 Werner Schweer (ws@seh.de)
//=========================================================

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#include <QFrame>
#include <QColor>
#include <QGradient>
#include <QLinearGradient>
#include <QPointF>

#include "utils.h"

//---------------------------------------------------------
//   curTime
//---------------------------------------------------------

double curTime()
      {
      struct timeval t;
      gettimeofday(&t, 0);
      return (double)((double)t.tv_sec + (t.tv_usec / 1000000.0));
      }

//---------------------------------------------------------
//   dump
//    simple debug output
//---------------------------------------------------------

void dump(const unsigned char* p, int n)
      {
      printf("dump %d\n", n);
      for (int i = 0; i < n; ++i) {
            printf("%02x ", *p++);
            if ((i > 0) && (i % 16 == 0) && (i+1 < n))
                  printf("\n");
            }
      printf("\n");
      }

//---------------------------------------------------------
//   num2cols
//---------------------------------------------------------

int num2cols(int min, int max)
      {
      int amin = abs(min);
      int amax = abs(max);
      int l = amin > amax ? amin : amax;
      return int(log10(l)) + 1;
      }

//---------------------------------------------------------
//   hLine
//---------------------------------------------------------

QFrame* hLine(QWidget* w)
      {
      QFrame* delim = new QFrame(w);
      delim->setFrameStyle(QFrame::HLine | QFrame::Sunken);
      return delim;
      }

//---------------------------------------------------------
//   vLine
//---------------------------------------------------------

QFrame* vLine(QWidget* w)
      {
      QFrame* delim = new QFrame(w);
      delim->setFrameStyle(QFrame::VLine | QFrame::Sunken);
      return delim;
      }

//---------------------------------------------------------
//   bitmap2String
//    5c -> 1-4 1-6
//
//    01011100
//
//---------------------------------------------------------

QString bitmap2String(int bm)
      {
      QString s;
//printf("bitmap2string: bm %04x", bm);
      if (bm == 0xffff)
            s = "all";
      else if (bm == 0)
            s = "none";
      else {
            bool range = false;
            int first = 0;
            bool needSpace = false;
            bm &= 0xffff;
            for (int i = 0; i < 17; ++i) {
            //for (int i = 0; i < 16; ++i) {
                  if ((1 << i) & bm) {
                        if (!range) {
                              range = true;
                              first = i;
                              }
                        }
                  else {
                        if (range) {
                              if (needSpace)
                                    s += " ";
                              QString ns;
                              if (first == i-1)
                                    ns.sprintf("%d", first+1);
                              else
                                    ns.sprintf("%d-%d", first+1, i);
                              s += ns;
                              needSpace = true;
                              }
                        range = false;
                        }
                  }
            }
//printf(" -> <%s>\n", s.toLatin1());
      return s;
      }

//---------------------------------------------------------
//   u32bitmap2String
//---------------------------------------------------------
// Added by Tim. p3.3.8

QString u32bitmap2String(unsigned int bm)
      {
      QString s;
//printf("bitmap2string: bm %04x", bm);
      //if (bm == 0xffff)
      if (bm == 0xffffffff)
            s = "all";
      else if (bm == 0)
            s = "none";
      else {
            bool range = false;
            int first = 0;
            //unsigned int first = 0;
            bool needSpace = false;
            //bm &= 0xffff;
            //for (int i = 0; i < 17; ++i) {
            for (int i = 0; i < 33; ++i) {
                  if ((i < 32) && ((1U << i) & bm)) {
                        if (!range) {
                              range = true;
                              first = i;
                              }
                        }
                  else {
                        if (range) {
                              if (needSpace)
                                    s += " ";
                              QString ns;
                              if (first == i-1)
                                    ns.sprintf("%d", first+1);
                                    //ns.sprintf("%u", first+1);
                              else
                                    ns.sprintf("%d-%d", first+1, i);
                                    //ns.sprintf("%u-%u", first+1, i);
                              s += ns;
                              needSpace = true;
                              }
                        range = false;
                        }
                  }
            }
//printf(" -> <%s>\n", s.toLatin1());
      return s;
      }

//---------------------------------------------------------
//   string2bitmap
//---------------------------------------------------------

int string2bitmap(const QString& str)
      {
      int val = 0;
      QString ss = str.simplified();
      QByteArray ba = ss.toLatin1();
      const char* s = ba.constData();
//printf("string2bitmap <%s>\n", s);

      if (s == 0)
            return 0;
      if (strcmp(s, "all") == 0)
            return 0xffff;
      if (strcmp(s, "none") == 0)
            return 0;
// printf("str2bitmap: <%s> ", str.toLatin1);
      int tval   = 0;
      bool range = false;
      int sval   = 0;
      while (*s == ' ')
            ++s;
      while (*s) {
            if (*s >= '0'  && *s <= '9') {
                  tval *= 10;
                  tval += *s - '0';
                  }
            else if (*s == ' ' || *s == ',') {
                  if (range) {
                        for (int i = sval-1; i < tval; ++i)
                              val |= (1 << i);
                        range = false;
                        }
                  else {
                        val |= (1 << (tval-1));
                        }
                  tval = 0;
                  }
            else if (*s == '-') {
                  range = true;
                  sval  = tval;
                  tval  = 0;
                  }
            ++s;
            }
      if (range && tval) {
            for (int i = sval-1; i < tval; ++i)
                  val |= (1 << i);
            }
      else if (tval) {
            val |= (1 << (tval-1));
            }
      return val & 0xffff;
      }

//---------------------------------------------------------
//   string2u32bitmap
//---------------------------------------------------------
// Added by Tim. p3.3.8

unsigned int string2u32bitmap(const QString& str)
      {
      //int val = 0;
      unsigned int val = 0;
      QString ss = str.simplified();
      QByteArray ba = ss.toLatin1();
      const char* s = ba.constData();
//printf("string2bitmap <%s>\n", s);

      if (s == 0)
            return 0;
      if (strcmp(s, "all") == 0)
            //return 0xffff;
            return 0xffffffff;
      if (strcmp(s, "none") == 0)
            return 0;
// printf("str2bitmap: <%s> ", str.toLatin1);
      int tval   = 0;
      //unsigned int tval   = 0;
      bool range = false;
      int sval   = 0;
      //unsigned int sval   = 0;
      while (*s == ' ')
            ++s;
      while (*s) {
            if (*s >= '0'  && *s <= '9') {
                  tval *= 10;
                  tval += *s - '0';
                  }
            else if (*s == ' ' || *s == ',') {
                  if (range) {
                        for (int i = sval-1; i < tval; ++i)
                        //for (unsigned int i = sval-1; i < tval; ++i)
                              val |= (1U << i);
                        range = false;
                        }
                  else {
                        val |= (1U << (tval-1));
                        }
                  tval = 0;
                  }
            else if (*s == '-') {
                  range = true;
                  sval  = tval;
                  tval  = 0;
                  }
            ++s;
            }
      if (range && tval) {
            for (int i = sval-1; i < tval; ++i)
            //for (unsigned int i = sval-1; i < tval; ++i)
                  val |= (1U << i);
            }
      else if (tval) {
            val |= (1U << (tval-1));
            }
      //return val & 0xffff;
      return val;
      }

//---------------------------------------------------------
//   autoAdjustFontSize
//   w: Widget to auto adjust font size
//   s: String to fit
//   ignoreWidth: Set if dealing with a vertically constrained widget - one which is free to resize horizontally.
//   ignoreHeight: Set if dealing with a horizontally constrained widget - one which is free to resize vertically. 
//---------------------------------------------------------
// Added by Tim. p3.3.8

bool autoAdjustFontSize(QFrame* w, const QString& s, bool ignoreWidth, bool ignoreHeight, int max, int min)
{
  // In case the max or min was obtained from QFont::pointSize() which returns -1 
  //  if the font is a pixel font, or if min is greater than max...
  if(!w || (min < 0) || (max < 0) || (min > max))
    return false;
    
  // Limit the minimum and maximum sizes to something at least readable.
  if(max < 4)
    max = 4;
  if(min < 4)
    min = 4;
    
  QRect cr = w->contentsRect();
  QRect r;
  QFont fnt = w->font();
  // An extra amount just to be sure - I found it was still breaking up two words which would fit on one line.
  int extra = 4;
  // Allow at least one loop. min can be equal to max.
  for(int i = max; i >= min; --i)
  {
    fnt.setPointSize(i);
    QFontMetrics fm(fnt);
    r = fm.boundingRect(s);
    // Would the text fit within the widget?
    if((ignoreWidth || (r.width() <= (cr.width() - extra))) && (ignoreHeight || (r.height() <= cr.height())))
      break;
  }
  // Added by Tim. p3.3.9
  //printf("autoAdjustFontSize: ptsz:%d widget:%s before setFont x:%d y:%d w:%d h:%d\n", fnt.pointSize(), w->name(), w->x(), w->y(), w->width(), w->height());
  
  // Here we will always have a font ranging from min to max point size.
  w->setFont(fnt);
  // Added by Tim. p3.3.9
  //printf("autoAdjustFontSize: ptsz:%d widget:%s x:%d y:%d w:%d h:%d frame w:%d rw:%d rh:%d\n", fnt.pointSize(), w->name(), w->x(), w->y(), w->width(), w->height(), w->frameWidth(), cr.width(), cr.height());
  
  // Force minimum height. Use the expected height for the highest given point size.
  // This way the mixer strips aren't all different label heights, but can be larger if necessary.
  // Only if ignoreHeight is set (therefore the height is adjustable).
  if(ignoreHeight)
  {
    fnt.setPointSize(max);
    QFontMetrics fm(fnt);
    // Set the label's minimum height equal to the height of the font.
    w->setMinimumHeight(fm.height() + 2 * w->frameWidth());
  }
  
  return true;  
}

QGradient gGradientFromQColor(const QColor& c, const QPointF& start, const QPointF& finalStop)
{
  int h, s, v, a;
  c.getHsv(&h, &s, &v, &a);
  const int v0 = v + (255 - v)/2;
  const int v1 = v - v/2;
  const QColor c0 = QColor::fromHsv(h, s, v0, a); 
  const QColor c1 = QColor::fromHsv(h, s, v1, a); 
  
  QLinearGradient gradient(start, finalStop);
  gradient.setColorAt(0, c0);
  gradient.setColorAt(1, c1);
  
  return gradient;
}