//=========================================================
//  MusE
//  Linux Music Editor
//    $Id: prcanvas.cpp,v 1.20.2.19 2009/11/16 11:29:33 lunar_shuttle Exp $
//  (C) Copyright 1999-2004 Werner Schweer (ws@seh.de)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; version 2 of
//  the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//=========================================================

#include <QApplication>
#include <QClipboard>
#include <QPainter>
#include <QDrag>
#include <QDragLeaveEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QList>
#include <QPair>

#include <set>

#include <limits.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>

#include "xml.h"
#include "prcanvas.h"
#include "midiport.h"
#include "event.h"
#include "mpevent.h"
#include "globals.h"
#include "cmd.h"
#include "song.h"
#include "audio.h"
#include "functions.h"
#include "gconfig.h"
#include "helper.h"

#define CHORD_TIMEOUT 75

namespace MusEGui {

//---------------------------------------------------------
//   NEvent
//---------------------------------------------------------

NEvent::NEvent(const MusECore::Event& e, MusECore::Part* p, int y) : MusEGui::CItem(e, p)
      {
      y = y - KH/4;
      unsigned tick = e.tick() + p->tick();
      setPos(QPoint(tick, y));
      setBBox(QRect(tick, y, e.lenTick(), KH/2));
      }


//---------------------------------------------------------
//   addItem
//---------------------------------------------------------

CItem* PianoCanvas::addItem(MusECore::Part* part, const MusECore::Event& event)
      {
      if (signed(event.tick())<0) {
            printf("ERROR: trying to add event before current part!\n");
            return NULL;
      }

      NEvent* ev = new NEvent(event, part, pitch2y(event.pitch()));
      items.add(ev);

      int diff = event.tick()-part->lenTick();
      if (diff > 0)  {// too short part? extend it
            part->setLenTick(part->lenTick()+diff);
            }
      
      return ev;
      }

//---------------------------------------------------------
//   PianoCanvas
//---------------------------------------------------------

PianoCanvas::PianoCanvas(MidiEditor* pr, QWidget* parent, int sx, int sy)
   : EventCanvas(pr, parent, sx, sy)
      {
      colorMode = 0;
      for (int i=0;i<128;i++) noteHeldDown[i]=false;
      supportsResizeToTheLeft = true;
      
      steprec=new MusECore::StepRec(noteHeldDown);
      
      songChanged(SC_TRACK_INSERTED);
      connect(MusEGlobal::song, SIGNAL(midiNote(int, int)), SLOT(midiNote(int,int)));
      }

PianoCanvas::~PianoCanvas()
{
  delete steprec;
}
//---------------------------------------------------------
//   pitch2y
//---------------------------------------------------------

int PianoCanvas::pitch2y(int pitch) const
      {
      int tt[] = {
            5, 13, 19, 26, 34, 44, 52, 58, 65, 71, 78, 85
            };
      int y = (75 * KH) - (tt[pitch%12] + (7 * KH) * (pitch/12));
      if (y < 0)
            y = 0;
      return y;
      }

//---------------------------------------------------------
//   y2pitch
//---------------------------------------------------------

int PianoCanvas::y2pitch(int y) const
      {
      const int total = (10 * 7 + 5) * KH;       // 75 Ganztonschritte
      y = total - y;
      int oct = (y / (7 * KH)) * 12;
      char kt[] = {
            0, 0, 0, 0, 0,  0,   0, 0, 0, 0,          // 5
            1, 1, 1,      1,   1, 1, 1,               // 13
            2, 2,         2,   2, 2, 2,               // 19
            3, 3, 3,      3,   3, 3, 3,               // 26
            4, 4, 4, 4,   4,   4, 4, 4, 4,            // 34
            5, 5, 5, 5,   5,   5, 5, 5, 5, 5,         // 43
            6, 6, 6,      6,   6, 6, 6,               // 52
            7, 7,         7,   7, 7, 7,               // 58
            8, 8, 8,      8,   8, 8, 8,               // 65
            9, 9,         9,   9, 9, 9,               // 71
            10, 10, 10,  10,   10, 10, 10,            // 78
            11, 11, 11, 11, 11,   11, 11, 11, 11, 11  // 87
            };
      return kt[y % 91] + oct;
      }

//---------------------------------------------------------
//   drawEvent
//    draws a note
//---------------------------------------------------------

void PianoCanvas::drawItem(QPainter& p, const MusEGui::CItem* item,
   const QRect& rect)
      {
      QRect r = item->bbox();
      if(!virt())
        r.moveCenter(map(item->pos()));
      
      //QRect rr = p.transform().mapRect(rect);  // Gives inconsistent positions. Source shows wrong operation for our needs.
      QRect rr = map(rect);                      // Use our own map instead.        
      QRect mer = map(r);                              
      
      QRect mr = rr & mer;
      if(mr.isNull())
        return;
      
      p.setPen(Qt::black);
      struct Triple {
            int r, g, b;
            };

      static Triple myColors [12] = {  // ddskrjp
            { 0xff, 0x3d, 0x39 },
            { 0x39, 0xff, 0x39 },
            { 0x39, 0x3d, 0xff },
            { 0xff, 0xff, 0x39 },
            { 0xff, 0x3d, 0xff },
            { 0x39, 0xff, 0xff },
            { 0xff, 0x7e, 0x7a },
            { 0x7a, 0x7e, 0xff },
            { 0x7a, 0xff, 0x7a },
            { 0xff, 0x7e, 0xbf },
            { 0x7a, 0xbf, 0xff },
            { 0xff, 0xbf, 0x7a }
            };

      QColor color;
      NEvent* nevent   = (NEvent*) item;
      MusECore::Event event = nevent->event();
      if (nevent->part() != curPart){
            if(item->isMoving()) 
              color = Qt::gray;
            else if(item->isSelected()) 
              color = Qt::black;
            else  
              color = Qt::lightGray;
      }      
      else {
            if (item->isMoving()) {
                    color = Qt::gray;
                }
            else if (item->isSelected()) {
                  color = Qt::black;
                  }
            else {
                  color.setRgb(0, 0, 255);
                  switch(colorMode) {
                        case 0:
                              break;
                        case 1:     // pitch
                              {
                              Triple* c = &myColors[event.pitch() % 12];
                              color.setRgb(c->r, c->g, c->b);
                              }
                              break;
                        case 2:     // velocity
                              {
                              int velo = event.velo();
                              if (velo < 64)
                                    color.setRgb(velo*4, 0, 0xff);
                              else
                                    color.setRgb(0xff, 0, (127-velo) * 4);
                              }
                              break;
                        }
                  }
            }
      
      bool wmtxen = p.worldMatrixEnabled();
      p.setWorldMatrixEnabled(false);
      int mx = mr.x();
      int my = mr.y();
      int mw = mr.width();
      int mh = mr.height();
      int mex = mer.x();
      int mey = mer.y();
      int mew = mer.width();
      int meh = mer.height();
      color.setAlpha(MusEGlobal::config.globalAlphaBlend);
      QBrush brush(color);
      p.fillRect(mr, brush);

      if(mex >= mx && mex <= mx + mw)
        p.drawLine(mex, my, mex, my + mh - 1);                       // The left edge
      if(mex + mew >= mx && mex + mew <= mx + mw)
        p.drawLine(mex + mew, my, mex + mew, my + mh - 1);           // The right edge
      if(mey >= my && mey <= my + mh)
        p.drawLine(mx, mey, mx + mw - 1, mey);                       // The top edge
      if(mey + meh >= my && mey + meh <= my + mh)
        p.drawLine(mx, mey + meh - 1, mx + mw - 1, mey + meh - 1);   // The bottom edge

      // print note name on the drawn notes
      if (MusEGlobal::config.showNoteNamesInPianoRoll) {
        QFont f(MusEGlobal::config.fonts[1]);

        f.setPointSize(f.pointSize() * 0.85);
        p.setFont(f);

        if (color.lightnessF() > 0.6f) {

          p.setPen(Qt::black);

        } else {

          p.setPen(Qt::white);

        }
        QString noteStr = MusECore::pitch2string(event.pitch());

        p.drawText(mer,Qt::AlignHCenter|Qt::AlignCenter, noteStr.toUpper());
      }


      p.setWorldMatrixEnabled(wmtxen);
      }

//---------------------------------------------------------
//   drawTopItem
//---------------------------------------------------------
void PianoCanvas::drawTopItem(QPainter& , const QRect&)
{}

//---------------------------------------------------------
//   drawMoving
//    draws moving items
//---------------------------------------------------------

void PianoCanvas::drawMoving(QPainter& p, const MusEGui::CItem* item, const QRect& rect)
    {
      QRect mr = QRect(item->mp().x(), item->mp().y() - item->height()/2, item->width(), item->height());
      mr = mr.intersected(rect);
      if(!mr.isValid())
        return;
      p.setPen(Qt::black);
      p.setBrush(Qt::NoBrush);
      p.drawRect(mr);
    }

//---------------------------------------------------------
//   viewMouseDoubleClickEvent
//---------------------------------------------------------

void PianoCanvas::viewMouseDoubleClickEvent(QMouseEvent* event)
      {
      if ((_tool != MusEGui::PointerTool) && (event->button() != Qt::LeftButton)) {
            mousePress(event);
            return;
            }
      }

//---------------------------------------------------------
//   moveCanvasItems
//---------------------------------------------------------

MusECore::Undo PianoCanvas::moveCanvasItems(MusEGui::CItemList& items, int dp, int dx, DragType dtype, bool rasterize)
{      
  if(editor->parts()->empty())
    return MusECore::Undo(); //return empty list
  
  MusECore::PartsToChangeMap parts2change;
  MusECore::Undo operations;  
  
  for(MusECore::iPart ip = editor->parts()->begin(); ip != editor->parts()->end(); ++ip)
  {
    MusECore::Part* part = ip->second;
    if(!part)
      continue;
    
    int npartoffset = 0;
    for(MusEGui::iCItem ici = items.begin(); ici != items.end(); ++ici) 
    {
      MusEGui::CItem* ci = ici->second;
      if(ci->part() != part)
        continue;
      
      int x = ci->pos().x() + dx;
      int y = pitch2y(y2pitch(ci->pos().y()) + dp);
      QPoint newpos = QPoint(x, y);
      if(rasterize)
        newpos = raster(newpos);
      
      // Test moving the item...
      NEvent* nevent = (NEvent*) ci;
      MusECore::Event event    = nevent->event();
      x              = newpos.x();
      if(x < 0)
        x = 0;
      int ntick = (rasterize ? editor->rasterVal(x) : x) - part->tick();
      if(ntick < 0)
        ntick = 0;
      int diff = ntick + event.lenTick() - part->lenTick();
      
      // If moving the item would require a new part size...
      if(diff > npartoffset)
        npartoffset = diff;
    }
        
    if(npartoffset > 0)
    {    
      MusECore::iPartToChange ip2c = parts2change.find(part);
      if(ip2c == parts2change.end())
      {
        MusECore::PartToChange p2c = {0, npartoffset};
        parts2change.insert(std::pair<MusECore::Part*, MusECore::PartToChange> (part, p2c));
      }
      else
        ip2c->second.xdiff = npartoffset;
    }
  }
  
  bool forbidden=false;
  for(MusECore::iPartToChange ip2c = parts2change.begin(); ip2c != parts2change.end(); ++ip2c)
  {
    MusECore::Part* opart = ip2c->first;
    if (opart->hasHiddenEvents())
    {
			forbidden=true;
			break;
		}
  }    

	
	if (!forbidden)
	{
		std::vector< MusEGui::CItem* > doneList;
		typedef std::vector< MusEGui::CItem* >::iterator iDoneList;
		
		for(MusEGui::iCItem ici = items.begin(); ici != items.end(); ++ici) 
		{
		        MusEGui::CItem* ci = ici->second;
			
			int x = ci->pos().x();
			int y = ci->pos().y();
			int nx = x + dx;
			int ny = pitch2y(y2pitch(y) + dp);
			QPoint newpos = QPoint(nx, ny);
			if(rasterize)
			  newpos = raster(newpos);
			selectItem(ci, true);
			
			iDoneList idl;
			for(idl = doneList.begin(); idl != doneList.end(); ++idl)
				// This compares EventBase pointers to see if they're the same...
				if((*idl)->event() == ci->event())
					break;
				
			// Do not process if the event has already been processed (meaning it's an event in a clone part)...
			if (idl == doneList.end())
			{
				moveItem(operations, ci, newpos, dtype, rasterize); // always returns true. if not, change is necessary here!
				doneList.push_back(ci);
			}
			ci->move(newpos);
						
			if(moving.size() == 1) 
						itemReleased(curItem, newpos);

			if(dtype == MOVE_COPY || dtype == MOVE_CLONE)
						selectItem(ci, false);
		}  

    for(MusECore::iPartToChange ip2c = parts2change.begin(); ip2c != parts2change.end(); ++ip2c)
    {
      MusECore::Part* opart = ip2c->first;
      int diff = ip2c->second.xdiff;
      
      schedule_resize_all_same_len_clone_parts(opart, opart->lenTick() + diff, operations);
    }    
					
  	return operations;
  }
  else
  {
		return MusECore::Undo(); //return empty list
	}
}
      
//---------------------------------------------------------
//   moveItem
//    called after moving an object
//---------------------------------------------------------

bool PianoCanvas::moveItem(MusECore::Undo& operations, MusEGui::CItem* item, const QPoint& pos, DragType dtype, bool rasterize)
      {
      NEvent* nevent = (NEvent*) item;
      MusECore::Event event    = nevent->event();
      int npitch     = y2pitch(pos.y());
      MusECore::Event newEvent = (dtype == MOVE_COPY) ? event.duplicate() : event.clone();
      int x          = pos.x();
      if (x < 0)
            x = 0;
      MusECore::Part* part = nevent->part();
      
      newEvent.setPitch(npitch);
      int ntick = (rasterize ? editor->rasterVal(x) : x) - part->tick();
      if (ntick < 0)
            ntick = 0;
      newEvent.setTick(ntick);
      newEvent.setLenTick(event.lenTick());

      // don't check, whether the new event is within the part
      // at this place. with operation groups, the part isn't
      // resized yet. (flo93)
      
      if (dtype == MOVE_COPY || dtype == MOVE_CLONE)
            operations.push_back(MusECore::UndoOp(MusECore::UndoOp::AddEvent, newEvent, part, false, false));
      else
            operations.push_back(MusECore::UndoOp(MusECore::UndoOp::ModifyEvent, newEvent, event, part, false, false));
      
      return true;
}

//---------------------------------------------------------
//   newItem(p, state)
//---------------------------------------------------------

MusEGui::CItem* PianoCanvas::newItem(const QPoint& p, int state)
      {
      int pitch = y2pitch(p.y());
      int tick = p.x();
      if(!(state & Qt::ShiftModifier))
        tick  = editor->rasterVal1(tick);
      int len   = p.x() - tick;
      tick     -= curPart->tick();
      if (tick < 0)
            //tick=0;
            return 0;
      MusECore::Event e =  MusECore::Event(MusECore::Note);
      e.setTick(tick);
      e.setPitch(pitch);
      e.setVelo(curVelo);
      e.setLenTick(len);

      NEvent *newEvent = new NEvent(e, curPart, pitch2y(pitch));
      if(_playEvents)
              startPlayEvent(e.pitch(), e.velo());
      return newEvent;
      }

void PianoCanvas::newItem(MusEGui::CItem* item, bool noSnap)
      {
      NEvent* nevent = (NEvent*) item;
      MusECore::Event event    = nevent->event();
      MusECore::Part* part = nevent->part();
      int ptick = part->tick();
      int x = item->x();
      if (x<ptick)
            x=ptick;
      if(!noSnap)
        x = editor->rasterVal1(x); //round down
      if (x<ptick)
            x=ptick;
      int w = item->width();
      event.setTick(x - ptick);
      if (!noSnap)
            w = editor->rasterVal(w);
      if (w == 0)
            w = editor->rasterStep(ptick);
      event.setLenTick(w);
            
      event.setPitch(y2pitch(item->y()));
      event.setSelected(true);

      MusECore::Undo operations;
      int diff = event.endTick()-part->lenTick();
      
      if (! ((diff > 0) && part->hasHiddenEvents()) ) //operation is allowed
      {
        operations.push_back(MusECore::UndoOp(MusECore::UndoOp::AddEvent,event, part, false, false));
        
        if (diff > 0)// part must be extended?
        {
              schedule_resize_all_same_len_clone_parts(part, event.endTick(), operations);
              printf("newItem: extending\n");
        }
        
        MusEGlobal::song->applyOperationGroup(operations);
      }
      else // forbid action by not applying it   
          songChanged(SC_EVENT_INSERTED); //this forces an update of the itemlist, which is necessary
                                          //to remove "forbidden" events from the list again
      }

//---------------------------------------------------------
//   resizeItem
//---------------------------------------------------------

void PianoCanvas::resizeItem(MusEGui::CItem* item, bool noSnap, bool rasterize)         // experimental changes to try dynamically extending parts
      {
      NEvent* nevent = (NEvent*) item;
      MusECore::Event event    = nevent->event();
      MusECore::Event newEvent = event.clone();
      int len;

      MusECore::Part* part = nevent->part();

      if (noSnap)
            len = nevent->width();
      else {
            unsigned tick = event.tick() + part->tick();
            len = editor->rasterVal(tick + nevent->width()) - tick;
            if (len <= 0)
                  len = editor->raster();
      }

      MusECore::Undo operations;
      int diff = event.tick()+len-part->lenTick();
      
      if((nevent->mp() != nevent->pos()) && (resizeDirection == RESIZE_TO_THE_LEFT))
      {
         int x = nevent->mp().x();
         int ntick = (rasterize ? editor->rasterVal(x) : x) - part->tick();
         if (ntick < 0)
               ntick = 0;
         newEvent.setTick(ntick);
      }

      if (! ((diff > 0) && part->hasHiddenEvents()) ) //operation is allowed
      {
        newEvent.setLenTick(len);
        operations.push_back(MusECore::UndoOp(MusECore::UndoOp::ModifyEvent,newEvent, event, nevent->part(), false, false));
        
        if (diff > 0)// part must be extended?
        {
              schedule_resize_all_same_len_clone_parts(part, event.tick()+len, operations);
              printf("resizeItem: extending\n");
        }
      }

      //else forbid action by not performing it
      MusEGlobal::song->applyOperationGroup(operations);
      songChanged(SC_EVENT_MODIFIED); //this forces an update of the itemlist, which is necessary
                                      //to remove "forbidden" events from the list again
      }

//---------------------------------------------------------
//   deleteItem
//---------------------------------------------------------

bool PianoCanvas::deleteItem(MusEGui::CItem* item)
      {
      NEvent* nevent = (NEvent*) item;
      if (nevent->part() == curPart) {
            MusECore::Event ev = nevent->event();
            // Indicate do undo, and do not do port controller values and clone parts. 
            MusEGlobal::audio->msgDeleteEvent(ev, curPart, true, false, false);
            return true;
            }
      return false;
      }

//---------------------------------------------------------
//   pianoCmd
//---------------------------------------------------------

void PianoCanvas::pianoCmd(int cmd)
      {
      switch(cmd) {
            case CMD_LEFT:
                  {
                  int spos = pos[0];
                  if(spos > 0) 
                  {
                    spos -= 1;     // Nudge by -1, then snap down with raster1.
                    spos = MusEGlobal::sigmap.raster1(spos, editor->rasterStep(pos[0]));
                  }  
                  if(spos < 0)
                    spos = 0;
                  MusECore::Pos p(spos,true);
                  MusEGlobal::song->setPos(0, p, true, true, true);
                  }
                  break;
            case CMD_RIGHT:
                  {
                  int spos = MusEGlobal::sigmap.raster2(pos[0] + 1, editor->rasterStep(pos[0]));    // Nudge by +1, then snap up with raster2.
                  MusECore::Pos p(spos,true);
                  MusEGlobal::song->setPos(0, p, true, true, true); 
                  }
                  break;
            case CMD_LEFT_NOSNAP:
                  {
                  int spos = pos[0] - editor->rasterStep(pos[0]);
                  if (spos < 0)
                        spos = 0;
                  MusECore::Pos p(spos,true);
                  MusEGlobal::song->setPos(0, p, true, true, true); //CDW
                  }
                  break;
            case CMD_RIGHT_NOSNAP:
                  {
                  MusECore::Pos p(pos[0] + editor->rasterStep(pos[0]), true);
                  MusEGlobal::song->setPos(0, p, true, true, true); //CDW
                  }
                  break;
            case CMD_INSERT:
                  {
                  if (pos[0] < start() || pos[0] >= end())
                        break;
                  MusECore::MidiPart* part = (MusECore::MidiPart*)curPart;

                  if (part == 0)
                        break;

                  MusECore::Undo operations;

                  std::list <MusECore::Event> elist;
                  for (MusECore::ciEvent e = part->events().lower_bound(pos[0] - part->tick()); e != part->events().end(); ++e)
                        elist.push_back((MusECore::Event)e->second);
                  for (std::list<MusECore::Event>::iterator i = elist.begin(); i != elist.end(); ++i) {
                        MusECore::Event event = *i;
                        MusECore::Event newEvent = event.clone();
                        newEvent.setTick(event.tick() + editor->raster());// - part->tick()); DELETETHIS
                        // Do not do port controller values and clone parts. 
                        operations.push_back(MusECore::UndoOp(MusECore::UndoOp::ModifyEvent, newEvent, event, part, false, false));
                        }
                  MusEGlobal::song->applyOperationGroup(operations);
                  
                  MusECore::Pos p(editor->rasterVal(pos[0] + editor->rasterStep(pos[0])), true);
                  MusEGlobal::song->setPos(0, p, true, false, true);
                  }
                  return;
            case CMD_BACKSPACE:
                  if (pos[0] < start() || pos[0] >= end())
                        break;
                  {
                  MusECore::MidiPart* part = (MusECore::MidiPart*)curPart;
                  if (part == 0)
                        break;
                  
                  MusECore::Undo operations;
                  std::list<MusECore::Event> elist;
                  for (MusECore::ciEvent e = part->events().lower_bound(pos[0]); e != part->events().end(); ++e)
                        elist.push_back((MusECore::Event)e->second);
                  for (std::list<MusECore::Event>::iterator i = elist.begin(); i != elist.end(); ++i) {
                        MusECore::Event event = *i;
                        MusECore::Event newEvent = event.clone();
                        newEvent.setTick(event.tick() - editor->raster() - part->tick());
                        // Do not do port controller values and clone parts. 
                        operations.push_back(MusECore::UndoOp(MusECore::UndoOp::ModifyEvent, newEvent, event, part, false, false));
                        }
                  MusEGlobal::song->applyOperationGroup(operations);
                  MusECore::Pos p(editor->rasterVal(pos[0] - editor->rasterStep(pos[0])), true);
                  MusEGlobal::song->setPos(0, p, true, false, true);
                  }
                  break;
            }
      }

//---------------------------------------------------------
//   pianoPressed
//---------------------------------------------------------

void PianoCanvas::pianoPressed(int pitch, int velocity, bool shift)
      {
      // REMOVE Tim. Noteoff. Added. Zero note on vel is not allowed now.
      if(velocity > 127)
        velocity = 127;
      else if(velocity <= 0)
        velocity = 1;
      
      // play note:
      if(_playEvents)
        startPlayEvent(pitch, velocity);
      
      if (_steprec && curPart) // && pos[0] >= start_tick && pos[0] < end_tick [removed by flo93: this is handled in steprec->record]
				 steprec->record(curPart,pitch,editor->raster(),editor->raster(),velocity,MusEGlobal::globalKeyState&Qt::ControlModifier,shift, -1 /* anything which is != rcSteprecNote */);
      }

//---------------------------------------------------------
//   pianoReleased
//---------------------------------------------------------

void PianoCanvas::pianoReleased(int /*pitch*/, bool)
      {
      // release key:
      if(_playEvents)
        stopPlayEvent();
      }

//---------------------------------------------------------
//   draw
//---------------------------------------------------------

void PianoCanvas::drawCanvas(QPainter& p, const QRect& rect)
      {
      int x = rect.x();
      int y = rect.y();
      int w = rect.width();
      int h = rect.height();

      
      //---------------------------------------------------
      //  horizontal lines
      //---------------------------------------------------

      int yy  = ((y-1) / KH) * KH + KH;
      int key = 75 - (yy / KH);
      
      
      for (; yy < y + h; yy += KH) {
            switch (key % 7) {
                  case 0:
                  case 3:
                        p.setPen(Qt::black);
                        p.drawLine(x, yy, x + w, yy);
                        break;
                  default:
                        p.fillRect(x, yy-3, w, 6, MusEGlobal::config.midiCanvasBg.darker(110));
                        break;
                  }
            --key;
            }
      
      //---------------------------------------------------
      // vertical lines
      //---------------------------------------------------

      drawTickRaster(p, x, y, w, h, editor->raster());
      
      }

//---------------------------------------------------------
//   cmd
//    pulldown menu commands
//---------------------------------------------------------

void PianoCanvas::cmd(int cmd)
      {
      switch (cmd) {
            case CMD_SELECT_ALL:     // select all
                  for (MusEGui::iCItem k = items.begin(); k != items.end(); ++k) {
                        if (!k->second->isSelected())
                              selectItem(k->second, true);
                        }
                  break;
            case CMD_SELECT_NONE:     // select none
                  deselectAll();
                  break;
            case CMD_SELECT_INVERT:     // invert selection
                  for (MusEGui::iCItem k = items.begin(); k != items.end(); ++k) {
                        selectItem(k->second, !k->second->isSelected());
                        }
                  break;
            case CMD_SELECT_ILOOP:     // select inside loop
                  for (MusEGui::iCItem k = items.begin(); k != items.end(); ++k) {
                        NEvent* nevent = (NEvent*)(k->second);
                        MusECore::Part* part     = nevent->part();
                        MusECore::Event event    = nevent->event();
                        unsigned tick  = event.tick() + part->tick();
                        if (tick < MusEGlobal::song->lpos() || tick >= MusEGlobal::song->rpos())
                              selectItem(k->second, false);
                        else
                              selectItem(k->second, true);
                        }
                  break;
            case CMD_SELECT_OLOOP:     // select outside loop
                  for (MusEGui::iCItem k = items.begin(); k != items.end(); ++k) {
                        NEvent* nevent = (NEvent*)(k->second);
                        MusECore::Part* part     = nevent->part();
                        MusECore::Event event    = nevent->event();
                        unsigned tick  = event.tick() + part->tick();
                        if (tick < MusEGlobal::song->lpos() || tick >= MusEGlobal::song->rpos())
                              selectItem(k->second, true);
                        else
                              selectItem(k->second, false);
                        }
                  break;
            case CMD_SELECT_PREV_PART:     // select previous part
                  {
                    MusECore::Part* pt = editor->curCanvasPart();
                    MusECore::Part* newpt = pt;
                    MusECore::PartList* pl = editor->parts();
                    for(MusECore::iPart ip = pl->begin(); ip != pl->end(); ++ip)
                      if(ip->second == pt) 
                      {
                        if(ip == pl->begin())
                          ip = pl->end();
                        --ip;
                        newpt = ip->second;
                        break;    
                      }
                    if(newpt != pt)
                      editor->setCurCanvasPart(newpt);
                  }
                  break;
            case CMD_SELECT_NEXT_PART:     // select next part
                  {
                    MusECore::Part* pt = editor->curCanvasPart();
                    MusECore::Part* newpt = pt;
                    MusECore::PartList* pl = editor->parts();
                    for(MusECore::iPart ip = pl->begin(); ip != pl->end(); ++ip)
                      if(ip->second == pt) 
                      {
                        ++ip;
                        if(ip == pl->end())
                          ip = pl->begin();
                        newpt = ip->second;
                        break;    
                      }
                    if(newpt != pt)
                      editor->setCurCanvasPart(newpt);
                  }
                  break;

            case CMD_FIXED_LEN:
            case CMD_CRESCENDO:
            case CMD_TRANSPOSE:
            case CMD_THIN_OUT:
            case CMD_ERASE_EVENT:
            case CMD_NOTE_SHIFT:
            case CMD_MOVE_CLOCK:
            case CMD_COPY_MEASURE:
            case CMD_ERASE_MEASURE:
            case CMD_DELETE_MEASURE:
            case CMD_CREATE_MEASURE:
                  break;
            default:
//                  printf("unknown ecanvas cmd %d\n", cmd);
                  break;
            }
      updateSelection();
      redraw();
      }

//---------------------------------------------------------
//   midiNote
//---------------------------------------------------------
void PianoCanvas::midiNote(int pitch, int velo)
      {
      if (MusEGlobal::debugMsg) printf("PianoCanvas::midiNote: pitch=%i, velo=%i\n", pitch, velo);

      if (velo)
        noteHeldDown[pitch]=true;
      else
        noteHeldDown[pitch]=false;

      if (MusEGlobal::heavyDebugMsg)
      {
        printf("  held down notes are: ");
        for (int i=0;i<128;i++)
          if (noteHeldDown[i])
            printf("%i ",i);
        printf("\n");
      }
      
      if (_midiin && _steprec && curPart
         && !MusEGlobal::audio->isPlaying() && velo && pos[0] >= start_tick
         /* && pos[0] < end_tick [removed by flo93: this is handled in steprec->record] */
         && !(MusEGlobal::globalKeyState & Qt::AltModifier)) {
					 steprec->record(curPart,pitch,editor->raster(),editor->raster(),velo,MusEGlobal::globalKeyState&Qt::ControlModifier,MusEGlobal::globalKeyState&Qt::ShiftModifier);
         }
      }


//---------------------------------------------------------
//   startDrag
//---------------------------------------------------------

void PianoCanvas::startDrag(MusEGui::CItem* /* item*/, DragType t)
{
   QMimeData* md = selected_events_to_mime(partlist_to_set(editor->parts()), 1);

   if (md)
   {
      // "Note that setMimeData() assigns ownership of the QMimeData object to the QDrag object.
      //  The QDrag must be constructed on the heap with a parent QWidget to ensure that Qt can
      //  clean up after the drag and drop operation has been completed. "
      QDrag* drag = new QDrag(this);
      drag->setMimeData(md);

      if (t == MOVE_COPY || t == MOVE_CLONE)
         drag->exec(Qt::CopyAction);
      else
         drag->exec(Qt::MoveAction);
   }
}

//---------------------------------------------------------
//   dragEnterEvent
//---------------------------------------------------------

void PianoCanvas::dragEnterEvent(QDragEnterEvent* event)
      {
      //event->accept(Q3TextDrag::canDecode(event));
      event->acceptProposedAction();  // TODO CHECK Tim.
      }

//---------------------------------------------------------
//   dragMoveEvent
//---------------------------------------------------------

void PianoCanvas::dragMoveEvent(QDragMoveEvent*)
      {
      }

//---------------------------------------------------------
//   dragLeaveEvent
//---------------------------------------------------------

void PianoCanvas::dragLeaveEvent(QDragLeaveEvent*)
      {
      }

//---------------------------------------------------------
//   itemPressed
//---------------------------------------------------------

void PianoCanvas::itemPressed(const MusEGui::CItem* item)
      {
      if (!_playEvents)
            return;
      MusECore::Event e = ((NEvent*)item)->event();
      startPlayEvent(e.pitch(), e.velo());
      }

//---------------------------------------------------------
//   itemReleased
//---------------------------------------------------------

void PianoCanvas::itemReleased(const MusEGui::CItem*, const QPoint&)
      {
      if (!_playEvents)
              return;
      stopPlayEvent();
      }

//---------------------------------------------------------
//   itemMoved
//---------------------------------------------------------

void PianoCanvas::itemMoved(const MusEGui::CItem* item, const QPoint& pos)
      {
      int npitch = y2pitch(pos.y());
      if(!track())
      {
        // Stop any playing notes:
        stopPlayEvent();
        return;
      }
      const int port = track()->outPort();
      const int channel = track()->outChannel();
      // Ignore if the note is already playing.
      if(stuckNoteExists(port, channel, npitch))
        return;
      
      // Stop any playing notes:
      stopPlayEvent();

      if(_playEvents)
      {
        if (moving.size() <= 1) { // items moving or curItem
            const MusECore::Event e = ((NEvent*)item)->event();
            // play note:
            startPlayEvent(npitch, e.velo());
            }
      }
      }

//---------------------------------------------------------
//   curPartChanged
//---------------------------------------------------------

void PianoCanvas::curPartChanged()
      {
      EventCanvas::curPartChanged();
      editor->setWindowTitle(getCaption());
      }

//---------------------------------------------------------
//   modifySelected
//---------------------------------------------------------

void PianoCanvas::modifySelected(MusEGui::NoteInfo::ValType type, int val, bool delta_mode)
      {
      QList< QPair<int,MusECore::Event> > already_done;
      MusECore::Undo operations;
      
      for (MusEGui::iCItem i = items.begin(); i != items.end(); ++i) {
            if (!(i->second->isSelected()))
                  continue;
            NEvent* e   = (NEvent*)(i->second);
            MusECore::Event event = e->event();
            if (event.type() != MusECore::Note)
                  continue;

            MusECore::MidiPart* part = (MusECore::MidiPart*)(e->part());
            
            if (already_done.contains(QPair<int,MusECore::Event>(part->clonemaster_sn(), event)))
              continue;
            
            MusECore::Event newEvent = event.clone();

            switch (type) {
                  case MusEGui::NoteInfo::VAL_TIME:
                        {
                        int newTime = val;
                        if(delta_mode)
                          newTime += event.tick();
                        else
                          newTime -= part->tick();
                        if (newTime < 0)
                           newTime = 0;
                        newEvent.setTick(newTime);
                        }
                        break;
                  case MusEGui::NoteInfo::VAL_LEN:
                        {
                        int len = val;
                        if(delta_mode)
                          len += event.lenTick();
                        if (len < 1)
                              len = 1;
                        newEvent.setLenTick(len);
                        }
                        break;
                  case MusEGui::NoteInfo::VAL_VELON:
                        {
                        int velo = val;
                        if(delta_mode)
                          velo += event.velo();
                        if (velo > 127)
                              velo = 127;
                        else if (velo < 0)
                              // REMOVE Tim. Noteoff. Changed. Zero note on vel is not allowed now.
//                               velo = 0;
                              velo = 1;
                        newEvent.setVelo(velo);
                        }
                        break;
                  case MusEGui::NoteInfo::VAL_VELOFF:
                        {
                        int velo = val;
                        if(delta_mode)
                          velo += event.veloOff();
                        if (velo > 127)
                              velo = 127;
                        else if (velo < 0)
                              velo = 0;
                        newEvent.setVeloOff(velo);
                        }
                        break;
                  case MusEGui::NoteInfo::VAL_PITCH:
                        {
                        int pitch = val;
                        if(delta_mode)
                          pitch += event.pitch();
                        if (pitch > 127)
                              pitch = 127;
                        else if (pitch < 0)
                              pitch = 0;
                        newEvent.setPitch(pitch);
                        }
                        break;
                  }
            operations.push_back(MusECore::UndoOp(MusECore::UndoOp::ModifyEvent, newEvent, event, part, false, false));
            already_done.append(QPair<int,MusECore::Event>(part->clonemaster_sn(), event));
            }
      MusEGlobal::song->applyOperationGroup(operations);
      }

//---------------------------------------------------------
//   resizeEvent
//---------------------------------------------------------

void PianoCanvas::resizeEvent(QResizeEvent* ev)
      {
      if (ev->size().width() != ev->oldSize().width())
            emit newWidth(ev->size().width());
      EventCanvas::resizeEvent(ev);
      }



} // namespace MusEGui
