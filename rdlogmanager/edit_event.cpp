// edit_event.cpp
//
// Edit a Rivendell Log Event
//
//   (C) Copyright 2002-2019 Fred Gleason <fredg@paravelsystems.com>
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License version 2 as
//   published by the Free Software Foundation.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public
//   License along with this program; if not, write to the Free Software
//   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//

#include <qcolordialog.h>
#include <qmessagebox.h>
#include <qpainter.h>

#include <rdcart_search_text.h>
#include <rdconf.h>
#include <rdescape_string.h>

#include "add_event.h"
#include "edit_event.h"
#include "edit_perms.h"
#include "globals.h"

//
// Icons
//
#include "../icons/play.xpm"
#include "../icons/rml5.xpm"

EditEvent::EditEvent(QString eventname,bool new_event,
		     std::vector<QString> *new_events,QWidget *parent)
  : RDDialog(parent)
{
  event_saved=false;
  event_name=eventname;
  event_new_event=new_event;
  event_new_events=new_events;
  event_event=new RDEvent(eventname);

  setWindowTitle("RDLogManager - "+tr("Editing Event")+" - "+
		 event_event->name());

  //
  // Fix the Window Size
  //
  setMinimumSize(sizeHint());
  setMaximumSize(sizeHint());

  //
  // Create Icons
  //
  event_playout_map=new QPixmap(play_xpm);
  event_macro_map=new QPixmap(rml5_xpm);

  //
  // Library Section
  //
  // Text Filter
  //
  event_lib_filter_edit=new QLineEdit(this);
  if(rda->station()->filterMode()==RDStation::FilterAsynchronous) {
    event_lib_filter_edit->setGeometry(55,2,CENTER_LINE-135,20);
    connect(event_lib_filter_edit,SIGNAL(returnPressed()),
	    this,SLOT(searchData()));
  }
  else {
    event_lib_filter_edit->setGeometry(55,2,CENTER_LINE-70,20);
  }
  connect(event_lib_filter_edit,SIGNAL(textChanged(const QString &)),
	  this,SLOT(filterChangedData(const QString &)));
  QLabel *label=new QLabel(event_lib_filter_edit,tr("Filter:"),this);
  label->setFont(labelFont());
  label->setGeometry(5,2,45,20);
  label->setAlignment(Qt::AlignVCenter|Qt::AlignRight);
  event_search_button=new QPushButton(tr("Search"),this);
  event_search_button->setFont(subButtonFont());
  event_search_button->setGeometry(CENTER_LINE-70,2,60,20);
  event_search_button->setDisabled(true);
  connect(event_search_button,SIGNAL(clicked()),this,SLOT(searchData()));
  event_search_button->
    setVisible(rda->station()->filterMode()==RDStation::FilterAsynchronous);

  //
  // Group Filter
  //
  event_group_box=new QComboBox(this);
  event_group_box->setGeometry(55,25,CENTER_LINE-70,20);
  connect(event_group_box,SIGNAL(activated(const QString &)),
	  this,SLOT(filterActivatedData(const QString &)));
  label=new QLabel(event_group_box,tr("Group:"),this);
  label->setFont(labelFont());
  label->setGeometry(5,25,45,20);
  label->setAlignment(Qt::AlignVCenter|Qt::AlignRight);

  //
  // Type Filter
  //
  event_lib_type_group=new QButtonGroup(this);
  connect(event_lib_type_group,SIGNAL(buttonClicked(int)),
	  this,SLOT(filterClickedData(int)));
  QRadioButton *rbutton=new QRadioButton(this);
  rbutton->setGeometry(55,55,15,15);
  event_lib_type_group->addButton(rbutton,0);
  label=new QLabel(rbutton,tr("All"),this);
  label->setFont(labelFont());
  label->setGeometry(75,55,30,15);
  label->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
  rbutton->setChecked(true);

  rbutton=new QRadioButton(this);
  rbutton->setGeometry(125,55,15,15);
  event_lib_type_group->addButton(rbutton,1);
  label=new QLabel(rbutton,tr("Audio Only"),this);
  label->setFont(labelFont());
  label->setGeometry(145,55,80,15);
  label->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);

  rbutton=new QRadioButton(this);
  rbutton->setGeometry(235,55,15,15);
  event_lib_type_group->addButton(rbutton,2);
  label=new QLabel(rbutton,tr("Macros Only"),this);
  label->setFont(labelFont());
  label->setGeometry(255,55,80,15);
  label->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);

  //
  // Cart List
  //
  event_lib_list=new LibListView(this);
  event_lib_list->setGeometry(10,80,CENTER_LINE-20,sizeHint().height()-300);
  event_lib_list->setAllColumnsShowFocus(true);
  event_lib_list->setItemMargin(5);
  event_lib_list->addColumn("");
  event_lib_list->addColumn(tr("Cart"));
  event_lib_list->addColumn(tr("Group"));
  event_lib_list->addColumn(tr("Length"));
  event_lib_list->setColumnAlignment(3,Qt::AlignRight);
  event_lib_list->addColumn(tr("Title"));
  event_lib_list->addColumn(tr("Artist"));
  event_lib_list->addColumn(tr("Start"));
  event_lib_list->addColumn(tr("End"));
  event_lib_list->addColumn(tr("Type"));
  connect(event_lib_list,SIGNAL(clicked(Q3ListViewItem *)),
	  this,SLOT(cartClickedData(Q3ListViewItem *)));
  
  //
  // Empty Cart Source
  //
  event_empty_cart=new RDEmptyCart(this);
  event_empty_cart->setGeometry(CENTER_LINE-227,sizeHint().height()-202,32,32);

  //
  // Cart Player
  //
  QString sql;
  RDSqlQuery *q;
  event_player = NULL;
  sql=QString("select ")+
    "OUTPUT_CARD,"+  // 00
    "OUTPUT_PORT,"+  // 01
    "START_CART,"+   // 02
    "END_CART "+     // 03
    "from RDLOGEDIT where "+
    "STATION=\""+RDEscapeString(rda->station()->name())+"\"";
  q=new RDSqlQuery(sql);
  if(q->first()) {
    event_player=
      new RDSimplePlayer(rda->cae(),rda->ripc(),q->value(0).toInt(),q->value(1).toInt(),
			 q->value(2).toUInt(),q->value(3).toUInt(),this);
    event_player->playButton()->
      setGeometry(CENTER_LINE-180,sizeHint().height()-210,80,50);
    event_player->stopButton()->setGeometry(CENTER_LINE-90,sizeHint().height()-210,80,50);
    event_player->stopButton()->setOnColor(Qt::red);
  }
  delete q;

  //
  // Remarks
  //
  event_remarks_edit=new QTextEdit(this);
  event_remarks_edit->
    setGeometry(10,sizeHint().height()-150,CENTER_LINE-20,140);
  event_remarks_edit->setTextFormat(Qt::PlainText);
  label=new QLabel(event_remarks_edit,tr("REMARKS"),this);
  label->setFont(labelFont());
  label->setGeometry(15,sizeHint().height()-165,100,15);
  label->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);

  //
  // Load Group List
  //
  sql="select NAME from GROUPS order by NAME";
  q=new RDSqlQuery(sql);
  event_group_box->insertItem(tr("ALL"));
  while(q->next()) {
    event_group_box->insertItem(q->value(0).toString());
  }
  delete q;

  RefreshLibrary();

  //
  // Pre-Position Log Section
  //
  event_position_header=new QLabel(tr("PRE-POSITION LOG"),this);
  event_position_header->setFont(labelFont());
  event_position_header->setGeometry(CENTER_LINE+15,10,200,16);

  event_position_box=new QCheckBox(this);
  event_position_box->setGeometry(CENTER_LINE+15,27,15,22);
  connect(event_position_box,SIGNAL(toggled(bool)),
	  this,SLOT(prepositionToggledData(bool)));
  event_position_label=
    new QLabel(event_position_box,tr("Cue to this event"),this);
  event_position_label->setFont(labelFont());
  event_position_label->setGeometry(CENTER_LINE+35,27,150,22);
  event_position_label->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
  event_position_edit=new QTimeEdit(this);
  event_position_edit->setGeometry(CENTER_LINE+144,26,60,22);
  event_position_edit->setDisplayFormat("mm:ss");
  event_position_unit=new QLabel(event_position_box,
     tr("before scheduled start.  (First cart will have a STOP transition.)"),
				 this);
  event_position_unit->setFont(labelFont());
  event_position_unit->setGeometry(CENTER_LINE+212,27,
				   sizeHint().width()-CENTER_LINE-227,22);
  event_position_unit->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);

  //
  // Timed Start Section
  //
  event_timetype_header=new QLabel(tr("TIMED START"),this);
  event_timetype_header->setFont(labelFont());
  event_timetype_header->setGeometry(CENTER_LINE+15,62,200,16);

  //
  // Time Type
  //
  event_timetype_box=new QCheckBox(this);
  event_timetype_box->setGeometry(CENTER_LINE+15,85,15,15);
  event_timetype_label=
    new QLabel(event_timetype_box,tr("Use hard start time"),this);
  event_timetype_label->setGeometry(CENTER_LINE+35,84,120,16);
  event_timetype_label->setFont(labelFont());
  event_timetype_label->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);

  //
  // Post Point
  //
  event_post_box=new QCheckBox(this);
  event_post_box->setGeometry(CENTER_LINE+35,108,15,15);
  event_post_label=new QLabel(event_post_box,tr("Make Post Point"),this);
  event_post_label->setGeometry(CENTER_LINE+55,107,95,16);
  event_post_label->setFont(labelFont());
  event_post_label->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);

  //
  // Grace Time
  //
  event_grace_groupbox=
    new QGroupBox(tr("Action If Previous Event Still Playing"),this);
  event_grace_groupbox->setFont(labelFont());
  event_grace_groupbox->
    setGeometry(CENTER_LINE+160,77,sizeHint().width()-CENTER_LINE-200,45);
  event_grace_group=new QButtonGroup(this);
  event_immediate_button=new QRadioButton(tr("Start immediately"),this);
  event_immediate_button->setGeometry(CENTER_LINE+170,100,160,15);
  event_immediate_button->setFont(subLabelFont());
  event_grace_group->addButton(event_immediate_button,0);
  event_next_button=new QRadioButton(tr("Make next"),this);
  event_next_button->setGeometry(CENTER_LINE+310,100,160,15);
  event_next_button->setFont(subLabelFont());
  event_grace_group->addButton(event_next_button,1);
  event_wait_button=new QRadioButton(tr("Wait up to"),this);
  event_wait_button->setGeometry(CENTER_LINE+420,100,160,15);
  event_wait_button->setFont(subLabelFont());
  event_grace_group->addButton(event_wait_button,2);

  event_grace_edit=new QTimeEdit(this);
  event_grace_edit->setGeometry(CENTER_LINE+500,95,60,20);
  event_grace_edit->setDisplayFormat("mm:ss");
  connect(event_timetype_box,SIGNAL(toggled(bool)),
	  this,SLOT(timeToggledData(bool)));
  connect(event_grace_group,SIGNAL(buttonClicked(int)),
	  this,SLOT(graceClickedData(int)));

  //
  // Transition Type
  //
  event_transtype_box=new QComboBox(this);
  event_transtype_box->setGeometry(CENTER_LINE+390,131,110,26);
  event_transtype_box->insertItem(tr("Play"));
  event_transtype_box->insertItem(tr("Segue"));
  event_transtype_box->insertItem(tr("Stop"));
  connect(event_transtype_box,SIGNAL(activated(int)),
	  this,SLOT(timeTransitionData(int)));
  event_time_label=
    new QLabel(event_transtype_box,
	       tr("Transition if previous event ends before start time:"),this);
  event_time_label->setGeometry(CENTER_LINE+15,131,370,26);
  event_time_label->setFont(labelFont());
  event_time_label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);

  //
  // Enforcing Length Section
  //
  label=new QLabel(tr("ENFORCING LENGTH"),this);
  label->setFont(labelFont());
  label->setGeometry(CENTER_LINE+15,162,200,16);

  event_autofill_box=new QCheckBox(this,"event_autofill_box");
  event_autofill_box->setGeometry(CENTER_LINE+100,182,15,15);
  label=new QLabel(event_autofill_box,tr("Use AutoFill"),this);
  label->setFont(labelFont());
  label->setGeometry(CENTER_LINE+120,184,150,15);
  label->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);

  event_autofill_slop_box=new QCheckBox(this);
  event_autofill_slop_box->setGeometry(CENTER_LINE+210,182,15,15);
  connect(event_autofill_slop_box,SIGNAL(toggled(bool)),
	  this,SLOT(autofillWarnToggledData(bool)));
  event_autofill_slop_label1=
    new QLabel(event_autofill_slop_box,tr("Warn if fill is over or under"),
	       this);
  event_autofill_slop_label1->setFont(subLabelFont());
  event_autofill_slop_label1->setGeometry(CENTER_LINE+230,184,170,15);
  event_autofill_slop_label1->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);

  event_autofill_slop_edit=new QTimeEdit(this);
  event_autofill_slop_edit->setGeometry(CENTER_LINE+470,179,60,22);
  event_autofill_slop_edit->setDisplayFormat("mm:ss");
  event_autofill_slop_label=
    new QLabel(event_autofill_slop_edit,tr("by at least"),this);
  event_autofill_slop_label->setGeometry(CENTER_LINE+400,184,65,15);
  event_autofill_slop_label->setFont(subLabelFont());
  event_autofill_slop_label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);

  event_timescale_box=new QCheckBox(this);
  event_timescale_box->setGeometry(CENTER_LINE+250,182,15,15);
  label=new QLabel(event_timescale_box,tr("Use Timescaling"),this);
  label->setFont(labelFont());
  label->setGeometry(CENTER_LINE+270,182,150,15);
  label->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
  event_timescale_box->hide();
  label->hide();

  //
  // Pre-Import Carts Section
  //
  label=new QLabel(tr("PRE-IMPORT CARTS"),this);
  label->setFont(labelFont());
  label->setGeometry(CENTER_LINE+15,210,200,16);

  //
  // Pre-Import Carts List
  //
  event_preimport_length_edit=new QLineEdit(this);
  event_preimport_length_edit->setGeometry(sizeHint().width()-140,208,80,20);
  event_preimport_length_edit->setReadOnly(true);
  label=new QLabel(event_preimport_length_edit,tr("Len:"),this);
  label->setFont(labelFont());
  label->setGeometry(sizeHint().width()-330,210,185,16);
  label->setAlignment(Qt::AlignVCenter|Qt::AlignRight);


  event_preimport_list=new ImportListView(this);
  event_preimport_list->setGeometry(CENTER_LINE+15,227,
				    sizeHint().width()-CENTER_LINE-75,125);
  event_preimport_list->setAllColumnsShowFocus(true);
  event_preimport_list->setItemMargin(5);
  event_preimport_list->load(event_name,RDEventImportList::PreImport);
  event_preimport_list->setSortColumn(-1);
  connect(event_preimport_list,SIGNAL(sizeChanged(int)),
	  this,SLOT(preimportChangedData(int)));
  event_preimport_list->addColumn("");
  event_preimport_list->addColumn(tr("Cart"));
  event_preimport_list->addColumn(tr("Group"));
  event_preimport_list->addColumn(tr("Length"));
  event_preimport_list->setColumnAlignment(3,Qt::AlignRight);
  event_preimport_list->addColumn(tr("Title"));
  event_preimport_list->addColumn(tr("Transition"));
  event_preimport_list->addColumn(tr("Count"));
  connect(event_preimport_list,SIGNAL(clicked(Q3ListViewItem *)),
	  this,SLOT(cartClickedData(Q3ListViewItem *)));
  connect(event_preimport_list,SIGNAL(lengthChanged(int)),
	  this,SLOT(preimportLengthChangedData(int)));
  event_preimport_up_button=new RDTransportButton(RDTransportButton::Up,this);
  event_preimport_up_button->setGeometry(sizeHint().width()-50,237,40,40);
  connect(event_preimport_up_button,SIGNAL(clicked()),
	  this,SLOT(preimportUpData()));
  event_preimport_down_button=
    new RDTransportButton(RDTransportButton::Down,this);
  event_preimport_down_button->setGeometry(sizeHint().width()-50,302,40,40);
  connect(event_preimport_down_button,SIGNAL(clicked()),
	  this,SLOT(preimportDownData()));

  //
  // Import Section
  //
  label=new QLabel(tr("IMPORT"),this);
  label->setFont(labelFont());
  label->setGeometry(CENTER_LINE+15,362,200,16);
  
  event_source_group=new QButtonGroup(this);
  connect(event_source_group,SIGNAL(buttonClicked(int)),
	  this,SLOT(importClickedData(int)));
  rbutton=new QRadioButton(this);
  event_source_group->addButton(rbutton,0);
  rbutton->setGeometry(CENTER_LINE+80,362,15,15);
  label=new QLabel(rbutton,tr("None"),this);
  label->setFont(labelFont());
  label->setGeometry(CENTER_LINE+100,362,150,15);
  label->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
  
  rbutton=new QRadioButton(this);
  event_source_group->addButton(rbutton,1);
  rbutton->setGeometry(CENTER_LINE+160,362,15,15);
  label=new QLabel(rbutton,tr("From Traffic"),this);
  label->setFont(labelFont());
  label->setGeometry(CENTER_LINE+180,362,150,15);
  label->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
  
  rbutton=new QRadioButton(this);
  rbutton->setGeometry(CENTER_LINE+280,362,15,15);
  event_source_group->addButton(rbutton,2);
  label=new QLabel(rbutton,tr("From Music"),this);
  label->setFont(labelFont());
  label->setGeometry(CENTER_LINE+300,362,150,15);
  label->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
  
  rbutton=new QRadioButton(this);
  rbutton->setGeometry(CENTER_LINE+400,362,15,15);
  event_source_group->addButton(rbutton,3);
  label=new QLabel(rbutton,tr("Select from:"),this);
  label->setFont(labelFont());
  label->setGeometry(CENTER_LINE+420,362,150,15);
  label->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
  
  //
  // Scheduler Group
  //
  event_sched_group_box=new QComboBox(this);
  event_sched_group_box->setGeometry(CENTER_LINE+510,359,100,20);
  QString sql2="select NAME from GROUPS order by NAME";
  RDSqlQuery *q2=new RDSqlQuery(sql2);
  while(q2->next()) {
    event_sched_group_box->insertItem(q2->value(0).toString());
  }
  delete q2;

  // 
  // Artist Separation SpinBox
  //
  event_artist_sep_label=new QLabel(tr("Artist Separation"),this);
  event_artist_sep_label->setFont(defaultFont());
  event_artist_sep_label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
  event_artist_sep_label->setGeometry(CENTER_LINE+410,383,100,20);
  
  event_artist_sep_spinbox=new QSpinBox(this);
  event_artist_sep_spinbox->setGeometry(CENTER_LINE+515,383,53,20);
  event_artist_sep_spinbox->setMinValue( -1 );
  event_artist_sep_spinbox->setMaxValue( 50000 );
  event_artist_sep_spinbox->setSpecialValueText("None");

  event_artist_none_button=new QPushButton(this);
  event_artist_none_button->setGeometry(CENTER_LINE+575,383,40,20);
  event_artist_none_button->setFont(subButtonFont());
  event_artist_none_button->setText(tr("None"));
  connect(event_artist_none_button,SIGNAL(clicked()),this,SLOT(artistData()));

  //
  // Title Separation SpinBox
  //
  event_title_sep_label=new QLabel(tr("Title Separation"),this);
  event_title_sep_label->setFont(defaultFont());
  event_title_sep_label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
  event_title_sep_label->setGeometry(CENTER_LINE+410,404,100,20);
  
  event_title_sep_spinbox=new QSpinBox(this);
  event_title_sep_spinbox->setGeometry(CENTER_LINE+515,404,53,20);
  event_title_sep_spinbox->setMinValue( -1 );
  event_title_sep_spinbox->setMaxValue( 50000 );
  event_title_sep_spinbox->setSpecialValueText("None");

  event_title_none_button=new QPushButton(this);
  event_title_none_button->setGeometry(CENTER_LINE+575,404,40,20);
  event_title_none_button->setFont(subButtonFont());
  event_title_none_button->setText(tr("None"));
  connect(event_title_none_button,SIGNAL(clicked()),this,SLOT(titleData()));

  //
  // Must have code..
  //
  event_have_code_label=new QLabel(tr("Must have code"),this);
  event_have_code_label->setFont(defaultFont());
  event_have_code_label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
  event_have_code_label->setGeometry(CENTER_LINE+410,427,100,20);
  
  event_have_code_box=new QComboBox(this);
  event_have_code_box->setGeometry(CENTER_LINE+515,427,100,20);

  //
  // And code
  //
  event_have_code2_label=new QLabel(tr("and code"),this);
  event_have_code2_label->setFont(defaultFont());
  event_have_code2_label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
  event_have_code2_label->setGeometry(CENTER_LINE+410,448,100,20);

  event_have_code2_box=new QComboBox(this);
  event_have_code2_box->setGeometry(CENTER_LINE+515,448,100,20);

  //
  // Fill scheduler codes
  //
  event_have_code_box->insertItem("[None]");
  event_have_code2_box->insertItem("[None]");

  sql2="select CODE from SCHED_CODES order by CODE";
  q2=new RDSqlQuery(sql2);
  while(q2->next()) {
    event_have_code_box->insertItem(q2->value(0).toString());
    event_have_code2_box->insertItem(q2->value(0).toString());
  }
  delete q2;

  //
  // Start Slop Time
  //
  event_startslop_label=new QLabel(tr("Import carts scheduled"),this);
  event_startslop_label->setFont(defaultFont());
  event_startslop_label->setGeometry(CENTER_LINE+30,383,140,22);
  event_startslop_label->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
  event_startslop_edit=new QTimeEdit(this);
  event_startslop_edit->setGeometry(CENTER_LINE+166,383,60,22);
  event_startslop_edit->setDisplayFormat("mm:ss");
  event_startslop_unit=new QLabel(tr("prior to the start of this event."),this);
  event_startslop_unit->setFont(defaultFont());
  event_startslop_unit->setGeometry(CENTER_LINE+230,383,
		     sizeHint().width()-CENTER_LINE-450,22);
  event_startslop_unit->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
  
  //
  // End Slop Time
  //
  event_endslop_label=new QLabel(tr("Import carts scheduled"),this);
  event_endslop_label->setFont(defaultFont());
  event_endslop_label->setGeometry(CENTER_LINE+30,404,140,22);
  event_endslop_label->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
  event_endslop_edit=new QTimeEdit(this);
  event_endslop_edit->setGeometry(CENTER_LINE+166,404,60,22);
  event_endslop_edit->setDisplayFormat("mm:ss");
  event_endslop_unit=new QLabel(tr("after the end of this event."),this);
  event_endslop_unit->setFont(defaultFont());
  event_endslop_unit->setGeometry(CENTER_LINE+230,404,
				  sizeHint().width()-CENTER_LINE-460,22);
  event_endslop_unit->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
  
  //
  // First Cart Transition Type
  //
  event_firsttrans_label=
    new QLabel(tr("First cart has a"),this);
  event_firsttrans_label->setFont(defaultFont());
  event_firsttrans_label->setGeometry(CENTER_LINE+30,428,140,22);
  event_firsttrans_label->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
  event_firsttrans_box=new QComboBox(this);
  event_firsttrans_box->setGeometry(CENTER_LINE+123,428,90,22);
  event_firsttrans_box->insertItem(tr("Play"));
  event_firsttrans_box->insertItem(tr("Segue"));
  event_firsttrans_box->insertItem(tr("Stop"));
  event_firsttrans_unit=new QLabel("transition.",this);
  event_firsttrans_unit->setFont(defaultFont());
  event_firsttrans_unit->setGeometry(CENTER_LINE+215,428,
		     sizeHint().width()-CENTER_LINE-450,22);
  event_firsttrans_unit->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
  
  //
  // Default Transition Type
  //
  event_defaulttrans_label=new QLabel(tr("Imported carts have a"),this);
  event_defaulttrans_label->setFont(defaultFont());
  event_defaulttrans_label->setGeometry(CENTER_LINE+30,451,180,22);
  event_defaulttrans_label->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
  event_defaulttrans_box=new QComboBox(this);
  event_defaulttrans_box->setGeometry(CENTER_LINE+163,451,90,22);
  event_defaulttrans_box->insertItem(tr("Play"));
  event_defaulttrans_box->insertItem(tr("Segue"));
  event_defaulttrans_box->insertItem(tr("Stop"));
  event_defaulttrans_unit=new QLabel(tr("transition."),this);
  event_defaulttrans_unit->setFont(defaultFont());
  event_defaulttrans_unit->setGeometry(CENTER_LINE+255,451,
		     sizeHint().width()-CENTER_LINE-420,22);
  event_defaulttrans_unit->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
  
  //
  // Nested Event
  //
  event_nestevent_label=new QLabel(tr("Import inline traffic with the"),this);
  event_nestevent_label->setFont(defaultFont());
  event_nestevent_label->setGeometry(CENTER_LINE+30,474,190,22);
  event_nestevent_label->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
  event_nestevent_box=new QComboBox(this);
  event_nestevent_box->setGeometry(CENTER_LINE+193,474,365,22);
  event_nestevent_box->insertItem(tr("[none]"));
  event_nestevent_unit=new QLabel(tr("event."),this);
  event_nestevent_unit->setFont(defaultFont());
  event_nestevent_unit->setGeometry(CENTER_LINE+563,474,40,22);
  event_nestevent_unit->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
  
  //
  // Post-Import Carts Section
  //
  label=new QLabel(tr("POST-IMPORT CARTS"),this);
  label->setFont(labelFont());
  label->setGeometry(CENTER_LINE+15,505,200,16);
  
  //
  // Post-Import Carts List
  //
  event_postimport_length_edit=new QLineEdit(this);
  event_postimport_length_edit->setGeometry(sizeHint().width()-140,503,80,20);
  event_postimport_length_edit->setReadOnly(true);
  label=new QLabel(event_postimport_length_edit,tr("Len:"),this);
  label->setFont(labelFont());
  label->setGeometry(sizeHint().width()-330,505,185,16);
  label->setAlignment(Qt::AlignVCenter|Qt::AlignRight);

  event_postimport_list=new ImportListView(this);
  event_postimport_list->setGeometry(CENTER_LINE+15,522,
				     sizeHint().width()-CENTER_LINE-75,125);
  event_postimport_list->setAllColumnsShowFocus(true);
  event_postimport_list->setItemMargin(5);
  event_postimport_list->setSortColumn(-1);
  event_postimport_list->setAllowStop(false);
  event_postimport_list->load(event_name,RDEventImportList::PostImport);
  event_postimport_list->addColumn("");
  event_postimport_list->addColumn(tr("Cart"));
  event_postimport_list->addColumn(tr("Group"));
  event_postimport_list->addColumn(tr("Length"));
  event_postimport_list->setColumnAlignment(3,Qt::AlignRight);
  event_postimport_list->addColumn(tr("Title"));
  event_postimport_list->addColumn(tr("Transition"));
  event_postimport_list->addColumn(tr("Count"));
  connect(event_postimport_list,SIGNAL(clicked(Q3ListViewItem *)),
	  this,SLOT(cartClickedData(Q3ListViewItem *)));
  connect(event_postimport_list,SIGNAL(lengthChanged(int)),
	  this,SLOT(postimportLengthChangedData(int)));
  event_postimport_up_button=
    new RDTransportButton(RDTransportButton::Up,this);
  event_postimport_up_button->setGeometry(sizeHint().width()-50,532,40,40);
  connect(event_postimport_up_button,SIGNAL(clicked()),
	  this,SLOT(postimportUpData()));
  event_postimport_down_button=
    new RDTransportButton(RDTransportButton::Down,this);
  event_postimport_down_button->setGeometry(sizeHint().width()-50,597,40,40);
  connect(event_postimport_down_button,SIGNAL(clicked()),
	  this,SLOT(postimportDownData()));
  
  //
  //  Save Button
  //
  QPushButton *button=new QPushButton(this);
  button->setGeometry(CENTER_LINE+10,sizeHint().height()-60,80,50);
  button->setFont(buttonFont());
  button->setText(tr("&Save"));
  connect(button,SIGNAL(clicked()),this,SLOT(saveData()));

  //
  //  Save As Button
  //
  button=new QPushButton(this);
  button->setGeometry(CENTER_LINE+100,sizeHint().height()-60,80,50);
  button->setFont(buttonFont());
  button->setText(tr("Save &As"));
  connect(button,SIGNAL(clicked()),this,SLOT(saveAsData()));

  //
  //  Service Association Button
  //
  button=new QPushButton(this);
  button->setGeometry(CENTER_LINE+(sizeHint().width()-CENTER_LINE)/2-85,
		      sizeHint().height()-60,80,50);
  button->setFont(buttonFont());
  button->setText(tr("&Services\nList"));
  connect(button,SIGNAL(clicked()),this,SLOT(svcData()));

  //
  //  Color Button
  //
  event_color_button=new QPushButton(this);
  event_color_button->
    setGeometry(CENTER_LINE+(sizeHint().width()-CENTER_LINE)/2+5,
		sizeHint().height()-60,80,50);
  event_color_button->setFont(buttonFont());
  event_color_button->setText(tr("C&olor"));
  connect(event_color_button,SIGNAL(clicked()),this,SLOT(colorData()));

  //
  //  OK Button
  //
  button=new QPushButton(this);
  button->setGeometry(sizeHint().width()-180,sizeHint().height()-60,80,50);
  if(rda->station()->filterMode()==RDStation::FilterSynchronous) {
    button->setDefault(true);
  }
  button->setFont(buttonFont());
  button->setText(tr("&OK"));
  connect(button,SIGNAL(clicked()),this,SLOT(okData()));

  //
  // Cancel Button
  //
  button=new QPushButton(this);
  button->setGeometry(sizeHint().width()-90,sizeHint().height()-60,80,50);
  button->setFont(buttonFont());
  button->setText(tr("&Cancel"));
  connect(button,SIGNAL(clicked()),this,SLOT(cancelData()));

  //
  // Load Event
  //
  event_remarks_edit->setText(event_event->remarks());
  int pos=event_event->preposition();
  if(pos<0) {
    event_position_box->setChecked(false);
    prepositionToggledData(false);
  }
  else {
    event_position_box->setChecked(true);
    event_position_edit->setTime(QTime().addMSecs(pos));
    prepositionToggledData(true);
  }
  
  int grace=0;
  switch(event_event->timeType()) {
  case RDLogLine::Relative:
    event_timetype_box->setChecked(false);
    event_grace_group->button(0)->setChecked(true);
    timeToggledData(false);
    break;
	
  case RDLogLine::Hard:
    event_timetype_box->setChecked(true);
    event_post_box->setChecked(event_event->postPoint());
    event_transtype_box->setCurrentItem(event_event->firstTransType());
    switch((grace=event_event->graceTime())) {
    case 0:
      event_grace_group->button(0)->setChecked(true);
      event_grace_edit->setTime(QTime());
      break;
	      
    case -1:
      event_grace_group->button(1)->setChecked(true);
      event_grace_edit->setTime(QTime());
      break;
	      
    default:
      event_grace_group->button(2)->setChecked(true);
      event_grace_edit->setTime(QTime().addMSecs(grace));
      break;
    }

  case RDLogLine::NoTime:
    break;
  }
  
  event_autofill_box->setChecked(event_event->useAutofill());
  int autofill_slop=event_event->autofillSlop();
  if(autofill_slop>=0) {
    event_autofill_slop_box->setChecked(true);
    event_autofill_slop_edit->setTime(QTime().addMSecs(autofill_slop));
  }
  autofillWarnToggledData(event_autofill_slop_box->isChecked());
  event_timescale_box->setChecked(event_event->useTimescale());
  event_source_group->button(event_event->importSource())->setChecked(true);
  event_startslop_edit->setTime(QTime().addMSecs(event_event->startSlop()));
  event_endslop_edit->setTime(QTime().addMSecs(event_event->endSlop()));
  event_firsttrans_box->setCurrentItem(event_event->firstTransType());
  event_defaulttrans_box->setCurrentItem(event_event->defaultTransType());
  if(!event_event->schedGroup().isEmpty()) {
    event_sched_group_box->setCurrentText(event_event->schedGroup());
  }
  event_artist_sep_spinbox->setValue(event_event->artistSep());
  event_title_sep_spinbox->setValue(event_event->titleSep());
  if(event_have_code_box->findText(event_event->HaveCode())!=-1) {
    event_have_code_box->setCurrentText(event_event->HaveCode());
  }
  if(event_have_code2_box->findText(event_event->HaveCode2())!=-1) {
    event_have_code2_box->setCurrentText(event_event->HaveCode2());
  }
  QColor color=event_event->color();
  if(color.isValid()) {
    event_color_button->setPalette(QPalette(color,backgroundColor()));
  }
  QString str=event_event->nestedEvent();
  sql=QString("select NAME from EVENTS where ")+
    "NAME!=\""+RDEscapeString(eventname)+"\""+
    "order by NAME";
  q=new RDSqlQuery(sql);
  while(q->next()) {
    event_nestevent_box->insertItem(q->value(0).toString());
    if(q->value(0).toString()==str) {
      event_nestevent_box->setCurrentItem(event_nestevent_box->count()-1);
    }
  }
  delete q;
  prepositionToggledData(event_position_box->isChecked());
  timeToggledData(event_timetype_box->isChecked());
  importClickedData(event_source_group->checkedId());
  preimportChangedData(event_preimport_list->childCount());
  SetPostTransition();
}


EditEvent::~EditEvent()
{
  delete event_lib_list;
  delete event_preimport_list;
  delete event_postimport_list;
}


QSize EditEvent::sizeHint() const
{
  return QSize(1024,715);
} 


QSizePolicy EditEvent::sizePolicy() const
{
  return QSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
}


void EditEvent::filterChangedData(const QString &str)
{
  if(rda->station()->filterMode()==RDStation::FilterSynchronous) {
    RefreshLibrary();
  }
  else {
    event_search_button->setEnabled(true);
  }
}


void EditEvent::filterActivatedData(const QString &str)
{
  if(rda->station()->filterMode()==RDStation::FilterSynchronous) {
    RefreshLibrary();
  }
  else {
    event_search_button->setEnabled(true);
  }
}


void EditEvent::filterClickedData(int id)
{
  if(rda->station()->filterMode()==RDStation::FilterSynchronous) {
    RefreshLibrary();
  }
  else {
    event_search_button->setEnabled(true);
  }
}


void EditEvent::searchData()
{
  RefreshLibrary();
  event_search_button->setDisabled(true);
}


void EditEvent::cartClickedData(Q3ListViewItem *item)
{
  if (!event_player) return;
  if(item==NULL) {
    event_player->setCart(0);
    return;
  }
  event_player->setCart(item->text(1).toUInt());
}


void EditEvent::prepositionToggledData(bool state)
{
  event_position_edit->setEnabled(state);
  event_timetype_box->setDisabled(state);
  event_timetype_header->setDisabled(state);
  event_timetype_label->setDisabled(state);
  event_preimport_list->setAllowFirstTrans(!state);
  if(event_timetype_box->isChecked()) {
    event_grace_groupbox->setDisabled(state);
    event_immediate_button->setDisabled(state);
    event_next_button->setDisabled(state);
    event_wait_button->setDisabled(state);
  }
  if((RDEventLine::ImportSource)event_source_group->checkedId()!=
     RDEventLine::None) {
    if(event_preimport_list->childCount()==0) {
      event_firsttrans_box->setDisabled(state);
      event_firsttrans_label->setDisabled(state);
      event_firsttrans_unit->setDisabled(state);
    }
  }
  if(state) {
    event_preimport_list->setForceTrans(RDLogLine::Stop);
  }
  else {
    if(event_timetype_box->isChecked()) {
      event_preimport_list->
	setForceTrans((RDLogLine::TransType)event_transtype_box->
		      currentItem());
    }
    else {
      event_preimport_list->setForceTrans(RDLogLine::NoTrans);
    }
  }
  event_preimport_list->refreshList();
  SetPostTransition();
}


void EditEvent::timeToggledData(bool state)
{
  event_grace_groupbox->setEnabled(state);
  event_immediate_button->setEnabled(state);
  event_next_button->setEnabled(state);
  event_wait_button->setEnabled(state);
  event_grace_edit->setEnabled(state);
  event_post_label->setEnabled(state&&(event_grace_group->checkedId()==0));
  event_post_box->setEnabled(state&&(event_grace_group->checkedId()==0));
  event_preimport_list->setAllowFirstTrans(!state);
  if(state) {
    event_preimport_list->
      setForceTrans((RDLogLine::TransType)event_transtype_box->currentItem());
  }
  else {
    if(event_position_box->isChecked()) {
      event_preimport_list->setForceTrans(RDLogLine::Stop);
    }
    else {
      event_preimport_list->setForceTrans(RDLogLine::NoTrans);
    }
  }
  event_preimport_list->refreshList();
  if(state) {
    graceClickedData(event_grace_group->checkedId());
    event_time_label->setEnabled(true);
    event_transtype_box->setEnabled(true);
    timeTransitionData(2);
    event_position_box->setDisabled(true);
    event_position_edit->setDisabled(true);
    event_position_header->setDisabled(true);
    event_position_label->setDisabled(true);
    event_position_unit->setDisabled(true);
    event_firsttrans_box->setDisabled(true);
    event_firsttrans_label->setDisabled(true);
    event_firsttrans_unit->setDisabled(true);
  }
  else {
    event_post_box->setChecked(false);
    event_grace_edit->setDisabled(true);
    event_time_label->setDisabled(true);
    event_transtype_box->setDisabled(true);
    if(event_position_box->isChecked()) {
      event_position_edit->setEnabled(true);
    }
    event_position_box->setEnabled(true);
    event_position_header->setEnabled(true);
    event_position_label->setEnabled(true);
    event_position_unit->setEnabled(true);
    if(((RDEventLine::ImportSource)event_source_group->checkedId()!=
       RDEventLine::None)&&(!event_position_box->isChecked())&&
       (event_preimport_list->childCount()==0)) {
      event_firsttrans_box->setEnabled(true);
      event_firsttrans_label->setEnabled(true);
      event_firsttrans_unit->setEnabled(true);
    }
  }
  SetPostTransition();
}


void EditEvent::graceClickedData(int id)
{
  switch(id) {
  case 0:
    event_post_label->setEnabled(event_timetype_box->isChecked());
    event_post_box->setEnabled(event_timetype_box->isChecked());
    timeTransitionData(RDLogLine::Stop);
    event_grace_edit->setDisabled(true);
    break;

  case 1:
    event_post_label->setDisabled(true);
    event_post_box->setDisabled(true);
    timeTransitionData(RDLogLine::Segue);
    event_grace_edit->setDisabled(true);
    break;

  case 2:
    event_post_label->setDisabled(true);
    event_post_box->setDisabled(true);
    timeTransitionData(RDLogLine::Segue);
    event_grace_edit->setEnabled(true);
    break;
  }
  SetPostTransition();
}


void EditEvent::timeTransitionData(int id)
{
  if(event_timetype_box->isChecked()) {
    event_preimport_list->
      setForceTrans((RDLogLine::TransType)event_transtype_box->currentItem());
  }
  else {
    event_preimport_list->setForceTrans(RDLogLine::NoTrans);
  }
  event_preimport_list->refreshList();
  SetPostTransition();
}


void EditEvent::autofillWarnToggledData(bool state)
{
  event_autofill_slop_edit->setEnabled(state);
  event_autofill_slop_label->setEnabled(state);
}


void EditEvent::importClickedData(int id)
{
  bool state=true;
  bool statesched=true;
  bool stateschedinv=false;
  if(id==0) {
    state=false;
    statesched=false;
  }
  if(id==3) {
    statesched=false;
    stateschedinv=true;
  }
  event_startslop_edit->setEnabled(statesched);
  event_startslop_label->setEnabled(statesched);
  event_startslop_unit->setEnabled(statesched);
  event_endslop_edit->setEnabled(statesched);
  event_endslop_label->setEnabled(statesched);
  event_endslop_unit->setEnabled(statesched);
  if((!event_timetype_box->isChecked())&&(!event_position_box->isChecked())) {
    if((state&&(event_preimport_list->childCount()==0))||(!state)) {
      event_firsttrans_box->setEnabled(state);
      event_firsttrans_label->setEnabled(state);
      event_firsttrans_unit->setEnabled(state);
    }
  }
  event_defaulttrans_box->setEnabled(state);
  event_defaulttrans_label->setEnabled(state);
  event_defaulttrans_unit->setEnabled(state);
  state=(id==2)&&state;
  event_nestevent_label->setEnabled(state);
  event_nestevent_box->setEnabled(state);
  event_nestevent_unit->setEnabled(state);
  SetPostTransition();
  event_sched_group_box->setEnabled(stateschedinv);
  event_artist_sep_label->setEnabled(stateschedinv);
  event_artist_sep_spinbox->setEnabled(stateschedinv);
  event_artist_none_button->setEnabled(stateschedinv);
  event_title_sep_label->setEnabled(stateschedinv);
  event_title_sep_spinbox->setEnabled(stateschedinv);
  event_title_none_button->setEnabled(stateschedinv);
  event_have_code_box->setEnabled(stateschedinv);
  event_have_code_label->setEnabled(stateschedinv);
  event_have_code2_box->setEnabled(stateschedinv);
  event_have_code2_label->setEnabled(stateschedinv);
}


void EditEvent::preimportChangedData(int size)
{
  if((size==0)&&(event_source_group->checkedId()!=0)&&
     (!event_position_box->isChecked())&&(!event_timetype_box->isChecked())) {
    event_firsttrans_box->setEnabled(true);
    event_firsttrans_label->setEnabled(true);
    event_firsttrans_unit->setEnabled(true);
  }
  else {
    event_firsttrans_box->setDisabled(true);
    event_firsttrans_label->setDisabled(true);
    event_firsttrans_unit->setDisabled(true);
  }
  SetPostTransition();
}


void EditEvent::preimportLengthChangedData(int msecs)
{
  event_preimport_length_edit->setText(RDGetTimeLength(msecs,true,false));
}


void EditEvent::preimportUpData()
{
  int line;
  Q3ListViewItem *item=event_preimport_list->selectedItem();
  if((item==NULL)||(item->text(6).isEmpty())) {
    return;
  }
  if((line=item->text(6).toInt())<1) {
    event_preimport_list->setSelected(item,true);
    event_preimport_list->ensureItemVisible(item);
    return;
  }
  event_preimport_list->move(line,line-1);
  event_preimport_list->validateTransitions();
  event_preimport_list->refreshList(line-1);
}


void EditEvent::preimportDownData()
{
  int line;
  Q3ListViewItem *item=event_preimport_list->selectedItem();
  if((item==NULL)||(item->text(6).isEmpty())) {
    return;
  }
  if((line=item->text(6).toInt())>=(event_preimport_list->childCount()-2)) {
    event_preimport_list->setSelected(item,true);
    event_preimport_list->ensureItemVisible(item);
    return;
  }
  event_preimport_list->move(line,line+1);
  event_preimport_list->validateTransitions();
  event_preimport_list->refreshList(line+1);
  event_preimport_list->ensureItemVisible(item);
}


void EditEvent::postimportUpData()
{
  int line;
  Q3ListViewItem *item=event_postimport_list->selectedItem();
  if(item==NULL) {
    return;
  }
  if((line=item->text(6).toInt())<1) {
    event_postimport_list->setSelected(item,true);
    event_postimport_list->ensureItemVisible(item);
    return;
  }
  event_postimport_list->move(line,line-1);
  event_postimport_list->validateTransitions();
  event_postimport_list->refreshList(line-1);
}


void EditEvent::postimportDownData()
{
  int line;
  Q3ListViewItem *item=event_postimport_list->selectedItem();
  if(item==NULL) {
    return;
  }
  if((line=item->text(6).toInt())>=(event_postimport_list->childCount()-1)) {
    event_postimport_list->setSelected(item,true);
    event_postimport_list->ensureItemVisible(item);
    return;
  }
  event_postimport_list->move(line,line+1);
  event_postimport_list->validateTransitions();
  event_postimport_list->refreshList(line+1);
}


void EditEvent::postimportLengthChangedData(int msecs)
{
  event_postimport_length_edit->setText(RDGetTimeLength(msecs,true,false));
}


void EditEvent::artistData()
{
  event_artist_sep_spinbox->setValue(-1);
}


void EditEvent::titleData()
{
  event_title_sep_spinbox->setValue(-1);
}


void EditEvent::saveData()
{
  Save();
  event_new_event=false;
}


void EditEvent::saveAsData()
{
  QString old_name;
  QString str;

  old_name=event_name;
  AddEvent *add_dialog=new AddEvent(&event_name,this);
  if(add_dialog->exec()<0) {
    delete add_dialog;
    return;
  }
  delete add_dialog;
  QString sql=QString("select NAME from EVENTS where ")+
    "NAME=\""+RDEscapeString(event_name)+"\"";
  RDSqlQuery *q=new RDSqlQuery(sql);
  if(!q->first()) {
    delete event_event;
    event_event=new RDEvent(event_name,true);
    Save();
    event_new_events->push_back(event_name);
    CopyEventPerms(old_name,event_name);
    if(event_new_event) {
      AbandonEvent(old_name);
    }
    setCaption("RDLogManager - "+tr("Editing Event")+" - "+event_event->name());
  }
  else {
    if(QMessageBox::question(this,tr("RDLogManager"),tr("Event already exists!\nDo you want to overwrite it?"),QMessageBox::Yes,QMessageBox::No)!=
       QMessageBox::Yes) {
      return;
    }
    delete event_event;
    event_event=new RDEvent(event_name,true);
    Save();
    event_new_events->push_back(event_name);
    sql=QString("delete from EVENT_PERMS where ")+
      "EVENT_NAME=\""+RDEscapeString(event_name)+"\"";
    q=new RDSqlQuery(sql);
    delete q;
    CopyEventPerms(old_name,event_name);
    if(event_new_event) {
      AbandonEvent(old_name);
    }
    str=QString(tr("Edit Event"));
    setCaption("RDLogManager - "+tr("Edit Event")+" - "+event_event->name());
  }
}


void EditEvent::svcData()
{
  EditPerms *dialog=new EditPerms(event_name,EditPerms::ObjectEvent,this);
  dialog->exec();
  delete dialog;
}


void EditEvent::colorData()
{
  QColor color=
    QColorDialog::getColor(event_color_button->backgroundColor(),this);
  if(color.isValid()) {
    event_color_button->setPalette(QPalette(color,backgroundColor()));
  }
}


void EditEvent::okData()
{
  Save();
  if (event_player){
    event_player->stop();
  }

  done(0);
}


void EditEvent::cancelData()
{
  if (event_player){
    event_player->stop();
  }
  if(event_saved) {
    done(-1);
  }
  else {
    done(-2);
  }
}


void EditEvent::closeEvent(QCloseEvent *e)
{
  cancelData();
}


void EditEvent::paintEvent(QPaintEvent *e)
{
  QPainter *p=new QPainter(this);
  p->setPen(Qt::black);
  p->drawLine(CENTER_LINE,10,CENTER_LINE,sizeHint().height()-10);
  p->drawLine(CENTER_LINE+408,383,CENTER_LINE+408,465);
  p->end();
}


void EditEvent::RefreshLibrary()
{
  QString type_filter;

  switch(event_lib_type_group->checkedId()) {
  case 0:
    type_filter="((TYPE=1)||(TYPE=2)||(TYPE=3))";
    break;

  case 1:
    type_filter="((TYPE=1)||(TYPE=3))";
    break;

  case 2:
    type_filter="(TYPE=2)";
    break;
  }
  QString sql=QString("select ")+
    "TYPE,"+            // 00
    "NUMBER,"+          // 01
    "GROUP_NAME,"+      // 02
    "FORCED_LENGTH,"+   // 03
    "TITLE,"+           // 04
    "ARTIST,"+          // 05
    "START_DATETIME,"+  // 06
    "END_DATETIME "+    // 07
    "from CART ";
  QString group=event_group_box->currentText();
  if(group==QString(tr("ALL"))) {
    group="";
  }
  sql+=RDCartSearchText(event_lib_filter_edit->text(),group,"",false)+" && "+
    type_filter;
  RDSqlQuery *q=new RDSqlQuery(sql);
  Q3ListViewItem *item;
  event_lib_list->clear();
  while(q->next()) {
    item=new Q3ListViewItem(event_lib_list);
    switch((RDCart::Type)q->value(0).toInt()) {
    case RDCart::Audio:
      item->setPixmap(0,*event_playout_map);
      item->setText(8,tr("Audio"));
      break;

    case RDCart::Macro:
      item->setPixmap(0,*event_macro_map);
      item->setText(8,tr("Macro"));
      break;

    case RDCart::All:
      break;
    }
    item->setText(1,QString().sprintf("%06u",q->value(1).toInt()));
    item->setText(2,q->value(2).toString());
    item->setText(3,RDGetTimeLength(q->value(3).toInt(),false,false));
    item->setText(4,q->value(4).toString());
    item->setText(5,q->value(5).toString());
    if(!q->value(6).toDateTime().isNull()) {
      item->setText(6,q->value(7).toDateTime().toString("MM/dd/yyyy"));
    }
    if(!q->value(7).toDateTime().isNull()) {
      item->setText(7,q->value(7).toDateTime().toString("MM/dd/yyyy"));
    }
    else {
      item->setText(7,"TFN");
    }
  }
  delete q;
}


void EditEvent::SetPostTransition()
{
  if((event_position_box->isChecked())||
     (event_timetype_box->isChecked())||
     (event_preimport_list->childCount()!=0)||
     (event_source_group->checkedId()!=0)) {
    event_postimport_list->setAllowStop(false);
  }
  else {
    event_postimport_list->setAllowStop(true);
  }
  if(event_preimport_list->childCount()==0) {
    if(event_position_box->isChecked()) {
      event_postimport_list->setAllowFirstTrans(false);
      event_postimport_list->setForceTrans(RDLogLine::Stop);
    }
    else {
      if(event_timetype_box->isChecked()) {
	event_postimport_list->
	  setForceTrans((RDLogLine::TransType)event_transtype_box->
			currentItem());
	  event_postimport_list->setAllowFirstTrans(false);
      }
      else {
	event_postimport_list->setAllowFirstTrans(true);
	event_postimport_list->setForceTrans(RDLogLine::NoTrans);
      }
    }
  }
  else { 
    event_postimport_list->setAllowFirstTrans(true);
    event_postimport_list->setForceTrans(RDLogLine::NoTrans);
  }
  event_postimport_list->refreshList();
}


void EditEvent::Save()
{
  QString properties;

  event_event->setRemarks(event_remarks_edit->text());
  if(event_position_box->isChecked()) {
    event_event->setPreposition(QTime().msecsTo(event_position_edit->time()));
  }
  else {
    event_event->setPreposition(-1);
  }
  if(event_timetype_box->isChecked()) {
    event_event->setTimeType(RDLogLine::Hard);
    event_event->setPostPoint(event_post_box->isChecked());
    event_event->setFirstTransType((RDLogLine::TransType)
				   event_transtype_box->currentItem());
    switch(event_grace_group->checkedId()) {
    case 0:
      event_event->setGraceTime(0);
      break;

    case 1:
      event_event->setGraceTime(-1);
      break;

    default:
      event_event->setGraceTime(QTime().msecsTo(event_grace_edit->time()));
      break;	  
    }
  }
  else {
    event_event->setTimeType(RDLogLine::Relative);
    event_event->setGraceTime(0);
    event_event->setPostPoint(false);
    event_event->setFirstTransType(RDLogLine::Play);
  }

  event_event->setUseAutofill(event_autofill_box->isChecked());
  if(event_autofill_slop_box->isChecked()) {
    event_event->
      setAutofillSlop(QTime().msecsTo(event_autofill_slop_edit->time()));
  }
  else {
    event_event->setAutofillSlop(-1);
  }
  event_event->setUseTimescale(event_timescale_box->isChecked());
  event_event->
    setImportSource((RDEventLine::ImportSource)event_source_group->checkedId());
  event_event->setStartSlop(QTime().msecsTo(event_startslop_edit->time()));
  event_event->setEndSlop(QTime().msecsTo(event_endslop_edit->time()));
  if(!event_timetype_box->isChecked()) {
    event_event->
      setFirstTransType((RDLogLine::TransType)event_firsttrans_box->
			currentItem());
  }
  event_event->
    setDefaultTransType((RDLogLine::TransType)event_defaulttrans_box->
			currentItem());
  event_event->setColor(event_color_button->backgroundColor());
  if(event_nestevent_box->currentItem()==0) {
    event_event->setNestedEvent("");
  }
  else {
    event_event->setNestedEvent(event_nestevent_box->currentText());
  }
  event_event->setProperties(GetProperties());
  event_event->setSchedGroup(event_sched_group_box->currentText());  
  event_event->setArtistSep(event_artist_sep_spinbox->value());
  event_event->setTitleSep(event_title_sep_spinbox->value());
  event_event->setHaveCode("");
  event_event->setHaveCode2("");
  if(event_have_code_box->currentIndex()>0) {
    event_event->setHaveCode(event_have_code_box->currentText());
  }
  if(event_have_code2_box->currentIndex()>0) {
    event_event->setHaveCode2(event_have_code2_box->currentText());
  }

  //
  // If both codes are the same, remove second code
  //
  if (event_event->HaveCode()==event_event->HaveCode2()) {
    event_event->setHaveCode2("");
  }

  //
  // Save second code as first code when first code isn't defined
  //
  if (event_event->HaveCode().isEmpty()) {
    event_event->setHaveCode(event_event->HaveCode2());
    event_event->setHaveCode2("");
  }

  event_preimport_list->setEventName(event_name);
  event_preimport_list->save();
  event_postimport_list->setEventName(event_name);
  event_postimport_list->save();

  event_saved=true;
}


QString EditEvent::GetProperties()
{
  QString properties;
  RDLogLine::TransType trans_type;
  QString str;

  if(event_position_box->isChecked()) {
    properties+=tr("Cue")+
      "(-"+event_position_edit->time().toString("mm:ss")+")";
  }
  if(event_timetype_box->isChecked()) {
    trans_type=(RDLogLine::TransType)event_transtype_box->currentItem();
  }
  else {
    if(event_preimport_list->childCount()>0) {
      str=event_preimport_list->firstChild()->text(5);
      trans_type=RDLogLine::Play;
      if(str==tr("SEGUE")) {
	trans_type=RDLogLine::Segue;
      }
      if(str==tr("STOP")) {
	trans_type=RDLogLine::Stop;
      }
    }
    else {
      if(event_position_box->isChecked()) {
	trans_type=RDLogLine::Stop;
      }
      else {
	trans_type=(RDLogLine::TransType)event_firsttrans_box->currentItem();
      }
    }
  }
  switch(trans_type) {
  case RDLogLine::Play:
    properties+=tr("PLAY");
    break;

  case RDLogLine::Segue:
    properties+=tr("SEGUE");
    break;

  case RDLogLine::Stop:
    properties+=tr("STOP");
    break;

  default:
    break;
  }

  if(event_timetype_box->isChecked()) {
    switch(event_grace_group->checkedId()) {
    case 0:
      properties+=tr(", Timed(Start)");
      break;

    case 1:
      properties+=tr(", Timed(MakeNext)");
      break;

    default:
      properties+=", "+tr("Timed(Wait)")+" "+
	event_grace_edit->time().toString("mm:ss");
      break;
    }
    if(event_post_box->isChecked()) {
      properties+=tr(", Post");
    }
  }
  if(event_autofill_box->isChecked()) {
    properties+=tr(", Fill");
  }
  if(event_timescale_box->isChecked()) {
    properties+=tr(", Scale");
  }
  switch((RDEventLine::ImportSource)event_source_group->checkedId()) {
  case RDEventLine::Traffic:
    properties+=tr(", Traffic");
    break;

  case RDEventLine::Music:
    properties+=tr(", Music");
    break;

  case RDEventLine::Scheduler:
    properties+=tr(", Scheduler");
    break;

  default:
    break;
  }
  if(event_nestevent_box->currentItem()>0) {
    properties+=tr(", Inline Traffic");
  }

  return properties;
}


void EditEvent::CopyEventPerms(QString old_name,QString new_name)
{
  QString sql;
  RDSqlQuery *q;
  RDSqlQuery *q1;

  sql=QString("select SERVICE_NAME from EVENT_PERMS where ")+
    "EVENT_NAME=\""+RDEscapeString(old_name)+"\"";
  q=new RDSqlQuery(sql);
  while(q->next()) {
    sql=QString("insert into EVENT_PERMS set ")+
      "EVENT_NAME=\""+RDEscapeString(new_name)+"\","+
      "SERVICE_NAME=\""+RDEscapeString(q->value(0).toString())+"\"";
    q1=new RDSqlQuery(sql);
    delete q1;
  }
  delete q;
}


void EditEvent::AbandonEvent(QString name)
{
  if(name==event_name) {
    return;
  }
  QString sql=QString("delete from EVENTS where ")+
    "NAME=\""+RDEscapeString(name)+"\"";
  RDSqlQuery *q=new RDSqlQuery(sql);
  delete q;
  sql=QString("delete from EVENT_PERMS where ")+
    "EVENT_NAME=\""+RDEscapeString(name)+"\"";
  q=new RDSqlQuery(sql);
  delete q;

  sql=QString("delete from EVENT_LINES where ")+
    "EVENT_NAME=\""+RDEscapeString(name)+"\"";
  RDSqlQuery::apply(sql);
}
