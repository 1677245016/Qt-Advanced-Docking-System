//============================================================================
/// \file   ContainerWidget.cpp
/// \author Uwe Kindler
/// \date   03.02.2017
/// \brief  Implementation of CContainerWidget
//============================================================================

//============================================================================
//                                   INCLUDES
//============================================================================
#include "ads/ContainerWidget.h"

#include <QDebug>
#include <QPaintEvent>
#include <QPainter>
#include <QContextMenuEvent>
#include <QMenu>
#include <QSplitter>
#include <QDataStream>
#include <QtGlobal>
#include <QGridLayout>
#include <QPoint>
#include <QApplication>

#include <iostream>

#include "ads/Internal.h"
#include "ads/SectionWidget.h"
#include "ads/SectionTitleWidget.h"
#include "ads/SectionContentWidget.h"
#include "ads/DropOverlay.h"
#include "ads/Serialization.h"
#include "ads/MainContainerWidget.h"

namespace ads
{
unsigned int CContainerWidget::zOrderCounter = 0;

//============================================================================
CContainerWidget::CContainerWidget(MainContainerWidget* MainContainer, QWidget *parent)
	: QFrame(parent),
	  m_MainContainerWidget(MainContainer)
{
	m_MainLayout = new QGridLayout();
	m_MainLayout->setContentsMargins(0, 1, 0, 0);
	m_MainLayout->setSpacing(0);
	setLayout(m_MainLayout);
}


//============================================================================
CContainerWidget::~CContainerWidget()
{
	std::cout << "CContainerWidget::~CContainerWidget()" << std::endl;
	m_MainContainerWidget->m_Containers.removeAll(this);
}


bool CContainerWidget::event(QEvent *e)
{
	bool Result = QWidget::event(e);
	if (e->type() == QEvent::WindowActivate)
    {
        m_zOrderIndex = ++zOrderCounter;
    }
	else if (e->type() == QEvent::Show && !m_zOrderIndex)
	{
		m_zOrderIndex = ++zOrderCounter;
	}

	return Result;
}


unsigned int CContainerWidget::zOrderIndex() const
{
	return m_zOrderIndex;
}

void CContainerWidget::dropFloatingWidget(FloatingWidget* FloatingWidget,
	const QPoint& TargetPos)
{
	SectionWidget* sectionWidget = sectionWidgetAt(TargetPos);
	DropArea dropArea = InvalidDropArea;
	if (sectionWidget)
	{
		auto dropOverlay = m_MainContainerWidget->sectionDropOverlay();
		dropOverlay->setAllowedAreas(ADS_NS::AllAreas);
		dropArea = dropOverlay->showDropOverlay(sectionWidget);
		if (dropArea != InvalidDropArea)
		{
			std::cout << "Section Drop Content: " << dropArea << std::endl;
			/*InternalContentData data;
			FloatingWidget->takeContent(data);
			FloatingWidget->deleteLater();
			dropContent(data, sectionWidget, dropArea, true);*/
			dropIntoSection(FloatingWidget, sectionWidget, dropArea);
		}
	}

	// mouse is over container
	if (InvalidDropArea == dropArea)
	{
		dropArea = m_MainContainerWidget->dropOverlay()->dropAreaUnderCursor();
		std::cout << "Container Drop Content: " << dropArea << std::endl;
		if (dropArea != InvalidDropArea)
		{
			// drop content
			dropIntoContainer(FloatingWidget, dropArea);
		}
	}
}


void CContainerWidget::dropChildSections(QWidget* Parent)
{
	auto Sections = Parent->findChildren<SectionWidget*>(QString(), Qt::FindDirectChildrenOnly);
	auto Splitters = Parent->findChildren<QSplitter*>(QString(), Qt::FindDirectChildrenOnly);

	std::cout << "-----------------------" << std::endl;
	std::cout << "Sections " << Sections.size() << std::endl;
	std::cout << "Splitters " << Splitters.size() << std::endl;

	for (auto Section : Sections)
	{
		// drop section
	}

	for (auto Splitter : Splitters)
	{
		dropChildSections(Splitter);
	}
}


void CContainerWidget::dropIntoContainer(FloatingWidget* FloatingWidget, DropArea area)
{
	dropChildSections(FloatingWidget->containerWidget());

	CContainerWidget* FloatingContainer = FloatingWidget->containerWidget();
	QSplitter* FloatingMainSplitter = FloatingContainer->findChild<QSplitter*>(QString(), Qt::FindDirectChildrenOnly);

	//QSplitter* oldsp = findImmediateSplitter(this);
	// We use findChild here instead of findImmediateSplitter because I do not
	// know what the advantage of the findImmediateSplitter function is
	QSplitter* OldSplitter = this->findChild<QSplitter*>(QString(), Qt::FindDirectChildrenOnly);
	//auto SectionWidgets = FloatingMainSplitter->findChildren<SectionWidget*>(QString(), Qt::FindDirectChildrenOnly);
	QList<SectionWidget*> SectionWidgets;
	for (int i = 0; i < FloatingMainSplitter->count(); ++i)
	{
		SectionWidgets.append(static_cast<SectionWidget*>(FloatingMainSplitter->widget(i)));
	}

	std::cout << "SectionWIdget[0] " << SectionWidgets[0] << " FloatingSplitter index 0"
		<< FloatingMainSplitter->widget(0) << std::endl;
	//std::cout<< "oldsp " << oldsp << " oldsp2 " << oldsp2 << std::endl;

	Qt::Orientation orientation;
	bool append;

	switch (area)
	{
	case TopDropArea: orientation = Qt::Vertical; append = false; break;
	case RightDropArea: orientation = Qt::Horizontal; append = true; break;
	case CenterDropArea:
	case BottomDropArea: orientation = Qt::Vertical; append = true; break;
	case LeftDropArea: orientation = Qt::Horizontal; append = false; break;
	}

	auto l = m_MainLayout;

	if (!OldSplitter)
	{
		std::cout << "Create new splitter" << std::endl;
		// we have no splitter yet - let us create one
		QSplitter* sp = MainContainerWidget::newSplitter(FloatingMainSplitter->orientation());
		if (l->count() > 0)
		{
			qWarning() << "Still items in layout. This should never happen.";
			QLayoutItem* li = l->takeAt(0);
			delete li;
		}
		l->addWidget(sp);
		for (auto SectionWidget : SectionWidgets)
		{
			sp->addWidget(SectionWidget);
		}
	}
	else if ((FloatingMainSplitter->orientation() == orientation) &&
		     (OldSplitter->count() == 1 || OldSplitter->orientation() == orientation))
	{
		OldSplitter->setOrientation(orientation);
		std::cout << "Splitter with right orientation" << std::endl;
		// we have a splitter with only one item or with the right orientation so
		// we can make it match the orientation of the floating splitter
		for (int i = 0; i < SectionWidgets.count(); ++i)
		{
			if (append)
			{
				OldSplitter->addWidget(SectionWidgets[i]);
			}
			else
			{
				OldSplitter->insertWidget(i, SectionWidgets[i]);
			}
		}
	}
	else
	{
		std::cout << "Splitter with wrong orientation" << std::endl;
		// we have a splitter but with the wrong orientation
		QSplitter* sp = MainContainerWidget::newSplitter(orientation);
		if (append)
		{
			QLayoutItem* li = l->replaceWidget(OldSplitter, sp);
			sp->addWidget(OldSplitter);
			sp->addWidget(FloatingMainSplitter);
			delete li;
		}
		else
		{
			sp->addWidget(FloatingMainSplitter);
			QLayoutItem* li = l->replaceWidget(OldSplitter, sp);
			sp->addWidget(OldSplitter);
			delete li;
		}
	}

	FloatingWidget->deleteLater();
}


void CContainerWidget::dropIntoSection(FloatingWidget* FloatingWidget,
	SectionWidget* targetSection, DropArea area)
{
	CContainerWidget* FloatingContainer = FloatingWidget->containerWidget();
	QSplitter* FloatingMainSplitter = FloatingContainer->findChild<QSplitter*>(QString(),
		Qt::FindDirectChildrenOnly);	QSplitter* OldSplitter = this->findChild<QSplitter*>(QString(), Qt::FindDirectChildrenOnly);
	QList<SectionWidget*> SectionWidgets;
	for (int i = 0; i < FloatingMainSplitter->count(); ++i)
	{
		SectionWidgets.append(static_cast<SectionWidget*>(FloatingMainSplitter->widget(i)));
	}

	switch (area)
	{
	/*case TopDropArea:return insertNewSectionWidget(data, targetSectionWidget, section_widget, Qt::Vertical, 0);
	case RightDropArea: return insertNewSectionWidget(data, targetSectionWidget, section_widget, Qt::Horizontal, 1);
	case BottomDropArea: return insertNewSectionWidget(data, targetSectionWidget, section_widget, Qt::Vertical, 1);
	case LeftDropArea: return insertNewSectionWidget(data, targetSectionWidget, section_widget, Qt::Horizontal, 0);*/
	case CenterDropArea:
		 for (auto SectionWidget : SectionWidgets)
		 {
			//sp->insertWidget(0, SectionWidget);
			//targetSectionWidget->addContent(data, autoActive);
			// targetSection->add
		 }
		 return;

	default:
		break;
	}

	/*QSplitter* targetSectionSplitter = findParentSplitter(targetSection);
	SectionWidget* sw = newSectionWidget();
	sw->addContent(data, true);
	if (targetSectionSplitter->orientation() == Orientation)
	{
		const int index = targetSectionSplitter->indexOf(targetSection);
		targetSectionSplitter->insertWidget(index + InsertIndexOffset, sw);
	}
	else
	{
		const int index = targetSectionSplitter->indexOf(targetSection);
		QSplitter* s = MainContainerWidget::newSplitter(Orientation);
		s->addWidget(sw);
		s->addWidget(targetSection);
		targetSectionSplitter->insertWidget(index, s);
	}
	ret = sw;
	return ret;*/
}



SectionWidget* CContainerWidget::sectionWidgetAt(const QPoint& pos) const
{
	for (const auto& SectionWidget : m_Sections)
	{
		if (SectionWidget->rect().contains(SectionWidget->mapFromGlobal(pos)))
		{
			return SectionWidget;
		}
	}

	return 0;
}


bool CContainerWidget::isInFrontOf(CContainerWidget* Other) const
{
	return this->m_zOrderIndex > Other->m_zOrderIndex;
}

SectionWidget*  CContainerWidget::dropContent(const InternalContentData& data, SectionWidget* targetSectionWidget, DropArea area, bool autoActive)
{
	ADS_Expects(targetSection != NULL);

	SectionWidget* section_widget = nullptr;

	// If no sections exists yet, create a default one and always drop into it.
	if (m_Sections.isEmpty())
	{
		targetSectionWidget = newSectionWidget();
		addSectionWidget(targetSectionWidget);
		area = CenterDropArea;
	}

	// Drop on outer area
	if (!targetSectionWidget)
	{
		switch (area)
		{
		case TopDropArea:return dropContentOuterHelper(m_MainLayout, data, Qt::Vertical, false);
		case RightDropArea: return dropContentOuterHelper(m_MainLayout, data, Qt::Horizontal, true);
		case CenterDropArea:
		case BottomDropArea:return dropContentOuterHelper(m_MainLayout, data, Qt::Vertical, true);
		case LeftDropArea: return dropContentOuterHelper(m_MainLayout, data, Qt::Horizontal, false);
		default:
			return nullptr;
		}
		return section_widget;
	}

	// Drop logic based on area.
	switch (area)
	{
	case TopDropArea:return insertNewSectionWidget(data, targetSectionWidget, section_widget, Qt::Vertical, 0);
	case RightDropArea: return insertNewSectionWidget(data, targetSectionWidget, section_widget, Qt::Horizontal, 1);
	case BottomDropArea: return insertNewSectionWidget(data, targetSectionWidget, section_widget, Qt::Vertical, 1);
	case LeftDropArea: return insertNewSectionWidget(data, targetSectionWidget, section_widget, Qt::Horizontal, 0);
	case CenterDropArea:
		 targetSectionWidget->addContent(data, autoActive);
		 return targetSectionWidget;

	default:
		break;
	}
	return section_widget;
}


SectionWidget* CContainerWidget::newSectionWidget()
{
	SectionWidget* sw = new SectionWidget(m_MainContainerWidget, this);
	m_Sections.append(sw);
	return sw;
}

void CContainerWidget::addSectionWidget(SectionWidget* section)
{
	ADS_Expects(section != NULL);

	// Create default splitter.
	if (!m_Splitter)
	{
		m_Splitter = MainContainerWidget::newSplitter(m_Orientation);
		m_MainLayout->addWidget(m_Splitter, 0, 0);
	}
	if (m_Splitter->indexOf(section) != -1)
	{
		qWarning() << Q_FUNC_INFO << QString("Section has already been added");
		return;
	}
	m_Splitter->addWidget(section);
}

SectionWidget* CContainerWidget::dropContentOuterHelper(QLayout* l, const InternalContentData& data, Qt::Orientation orientation, bool append)
{
	ADS_Expects(l != NULL);

	SectionWidget* sw = newSectionWidget();
	sw->addContent(data, true);

	QSplitter* oldsp = findImmediateSplitter(this);
	if (!oldsp)
	{
		QSplitter* sp = MainContainerWidget::newSplitter(orientation);
		if (l->count() > 0)
		{
			qWarning() << "Still items in layout. This should never happen.";
			QLayoutItem* li = l->takeAt(0);
			delete li;
		}
		l->addWidget(sp);
		sp->addWidget(sw);
	}
	else if (oldsp->orientation() == orientation
			|| oldsp->count() == 1)
	{
		oldsp->setOrientation(orientation);
		if (append)
			oldsp->addWidget(sw);
		else
			oldsp->insertWidget(0, sw);
	}
	else
	{
		QSplitter* sp = MainContainerWidget::newSplitter(orientation);
		if (append)
		{
			QLayoutItem* li = l->replaceWidget(oldsp, sp);
			sp->addWidget(oldsp);
			sp->addWidget(sw);
			delete li;
		}
		else
		{
			sp->addWidget(sw);
			QLayoutItem* li = l->replaceWidget(oldsp, sp);
			sp->addWidget(oldsp);
			delete li;
		}
	}
	return sw;
}

SectionWidget* CContainerWidget::insertNewSectionWidget(
    const InternalContentData& data, SectionWidget* targetSection, SectionWidget* ret,
    Qt::Orientation Orientation, int InsertIndexOffset)
{
	QSplitter* targetSectionSplitter = findParentSplitter(targetSection);
	SectionWidget* sw = newSectionWidget();
	sw->addContent(data, true);
	if (targetSectionSplitter->orientation() == Orientation)
	{
		const int index = targetSectionSplitter->indexOf(targetSection);
		targetSectionSplitter->insertWidget(index + InsertIndexOffset, sw);
	}
	else
	{
		const int index = targetSectionSplitter->indexOf(targetSection);
		QSplitter* s = MainContainerWidget::newSplitter(Orientation);
		s->addWidget(sw);
		s->addWidget(targetSection);
		targetSectionSplitter->insertWidget(index, s);
	}
	ret = sw;
	return ret;
}

SectionWidget* CContainerWidget::addSectionContent(const SectionContent::RefPtr& sc, SectionWidget* sw, DropArea area)
{
	ADS_Expects(!sc.isNull());

	// Drop it based on "area"
	InternalContentData data;
	data.content = sc;
	data.titleWidget = new SectionTitleWidget(sc, NULL);
	data.contentWidget = new SectionContentWidget(sc, NULL);

	connect(data.titleWidget, SIGNAL(activeTabChanged()), this, SLOT(onActiveTabChanged()));
	return dropContent(data, sw, area, false);
}


void dumpChildSplitters(QWidget* Widget)
{
	QSplitter* ParentSplitter = dynamic_cast<QSplitter*>(Widget);
	auto Sections = Widget->findChildren<SectionWidget*>(QString(), Qt::FindDirectChildrenOnly);
	auto Splitters = Widget->findChildren<QSplitter*>(QString(), Qt::FindDirectChildrenOnly);

	std::cout << "-----------------------" << std::endl;
	std::cout << "Sections " << Sections.size() << std::endl;
	std::cout << "Splitters " << Splitters.size() << std::endl;
	for (const auto& Splitter : Splitters)
	{
		if (ParentSplitter)
		{
			std::cout << "Orientation " << Splitter->orientation() << " index " << ParentSplitter->indexOf(Splitter) << std::endl;
		}
		else
		{
			std::cout << "Orientation " << Splitter->orientation() << std::endl;
		}
		dumpChildSplitters(Splitter);
	}
}



void CContainerWidget::dumpLayout()
{
	dumpChildSplitters(this);
}

void CContainerWidget::onActiveTabChanged()
{
	SectionTitleWidget* stw = qobject_cast<SectionTitleWidget*>(sender());
	if (stw)
	{
		emit activeTabChanged(stw->m_Content, stw->isActiveTab());
	}
}
} // namespace ads

//---------------------------------------------------------------------------
// EOF ContainerWidget.cpp
