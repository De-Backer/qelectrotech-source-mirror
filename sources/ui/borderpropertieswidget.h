/*
	Copyright 2006-2023 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech. If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef BORDERPROPERTIESWIDGET_H
#define BORDERPROPERTIESWIDGET_H

#include "../borderproperties.h"

#include <QWidget>

namespace Ui {
	class BorderPropertiesWidget;
}

/**
	@brief The BorderPropertiesWidget class
	this widget edit the properties of a border
*/
class BorderPropertiesWidget : public QWidget
{
		Q_OBJECT

	public:
		explicit BorderPropertiesWidget(const BorderProperties &bp, QWidget *parent = nullptr);
		~BorderPropertiesWidget() override;

		void setProperties(const BorderProperties &bp);
		const BorderProperties &properties();
		void setReadOnly (const bool &ro);

	private slots:

	private:
		Ui::BorderPropertiesWidget *ui;
		BorderProperties m_properties;
};

#endif // BORDERPROPERTIESWIDGET_H
