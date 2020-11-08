/*
	Copyright 2006-2020 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QObject>
#include <QHash>
#include "qetinformation.h"

/** Default information related to element **/
static QString ELMT_LABEL     = "label";
static QString ELMT_LABEL_VAR = "%{label}";

/** Default information related to conductor **/
static QString COND_FUNCTION             = "function";
static QString COND_FUNCTION_VAR         = "%{function}";
static QString COND_TENSION_PROTOCOL     = "tension/protocol";
static QString COND_TENSION_PROTOCOL_var = "%{tension/protocol}";
static QString COND_COLOR                = "conductor_color";
static QString COND_COLOR_VAR            = "%{conductor_color}";
static QString COND_SECTION              = "conductor_section";
static QString COND_SECTION_var          = "%{conductor_section}";

/**
	@brief QETInformation::titleblockInfoKeys
	@return all available key for use with a titleblock
*/
QStringList QETInformation::titleblockInfoKeys()
{
	QStringList info_list;
	info_list << "author";
	info_list << "date";
	info_list << "title";
	info_list << "filename";
	info_list << "plant";
	info_list << "locmach";
	info_list << "indexrev";
	info_list << "version";
	info_list << "folio";
	info_list << "folio-id";
	info_list << "folio-total";
	info_list << "previous-folio-num";
	info_list << "next-folio-num";
	info_list << "projecttitle";
	info_list << "projectpath";
	info_list << "projectfilename";
	info_list << "saveddate";
	info_list << "saveddate-eu";
	info_list << "saveddate-us";
	info_list << "savedtime";
	info_list << "savedfilename";
	info_list << "savedfilepath";

	return info_list;
}

/**
	@brief QETInformation::titleblockTranslatedInfoKey
	@param info : info key to be translated
	@return the translated information given by info
	If info don't match, return an empty string
*/
QString QETInformation::titleblockTranslatedInfoKey(const QString &info) {
	return translatedInfoKey(info);
}

/**
	@brief QETInformation::titleblockInfoKeysToVar
	@param info
	@return var in form %{my-var} corresponding to the info.
	if there is not available var for the given info, the returned var is %{void}
*/
QString QETInformation::titleblockInfoKeysToVar(const QString &info)
{
	if      (info == "author")             return QString("%{author}");
	else if (info == "date")               return QString("%{date}");
	else if (info == "title")              return QString("%{title}");
	else if (info == "filename")           return QString("%{filename}");
	else if (info == "plant")              return QString("%{plant}");
	else if (info == "locmach")            return QString("%{locmach}");
	else if (info == "indexrev")           return QString("%{indexrev}");
	else if (info == "version")            return QString("%{version}");
	else if (info == "folio")              return QString("%{folio}");
	else if (info == "folio-id")           return QString("%{folio-id}");
	else if (info == "folio-total")        return QString("%{folio-total}");
	else if (info == "previous-folio-num") return QString("%{previous-folio-num}");
	else if (info == "next-folio-num")     return QString("%{next-folio-num}");
	else if (info == "projecttitle")       return QString("%{projecttitle}");
	else if (info == "projectpath")        return QString("%{projectpath}");
	else if (info == "projectfilename")    return QString("%{projectfilename}");
	else if (info == "saveddate")          return QString("%{saveddate}");
	else if (info == "saveddate-eu")       return QString("%{saveddate-eu}");
	else if (info == "saveddate-us")       return QString("%{saveddate-us}");
	else if (info == "savedtime")          return QString("%{savedtime}");
	else if (info == "savedfilename")      return QString("%{savedfilename}");
	else if (info == "savedfilepath")      return QString("%{savedfilepath}");
	else return QString("%{void");
}

/**
	@brief QETInformation::titleblockTranslatedKeyHashVar
	@return a QHash with for key, the translated information key of title block,
	and for value the corresponding var.
*/
QHash<QString, QString> QETInformation::titleblockTranslatedKeyHashVar()
{
	QHash <QString, QString> hash_;
	for (QString str : titleblockInfoKeys()) {
		hash_.insert(titleblockTranslatedInfoKey(str), titleblockInfoKeysToVar(str));
	}
	return hash_;
}

/**
 * @brief QETInformation::folioReportInfoKey
 * @return The info key available for dynamic element text item of a folio report
 */
QStringList QETInformation::folioReportInfoKey()
{
	QStringList list;
	list << ELMT_LABEL;
	list << COND_FUNCTION;
	list << COND_TENSION_PROTOCOL;
	list << COND_COLOR;
	list << COND_SECTION;

	return list;
}
QHash<QString, QString> QETInformation::folioReportInfoKeyToVar()
{
	QHash <QString, QString> H_;
	H_.insert(ELMT_LABEL, ELMT_LABEL_VAR);
	H_.insert(COND_FUNCTION, COND_FUNCTION_VAR);
	H_.insert(COND_TENSION_PROTOCOL, COND_TENSION_PROTOCOL_var);
	H_.insert(COND_COLOR, COND_COLOR_VAR);
	H_.insert(COND_SECTION, COND_SECTION_var);

	return H_;
}

/**
 * @brief QETInformation::translatedInfoKey
 * @param info
 * @return the information translated to the current local.
 * This function translate all information : element, conductor, diagram
 * Return an empty QString if don't match
 */
QString QETInformation::translatedInfoKey(const QString &info)
{
	if      (info == "author")                         return QObject::tr("Auteur");
	else if (info == "date")                           return QObject::tr("Date");
	else if (info == "title")                          return QObject::tr("Titre");
	else if (info == "filename")                       return QObject::tr("Fichier");
	else if (info == "plant")                          return QObject::tr("Installation (=)");
	else if (info == "locmach")                        return QObject::tr("Localisation (+)");
	else if (info == "indexrev")                       return QObject::tr("Indice de révision");
	else if (info == "version")                        return QObject::tr("Version de QElectroTech");
	else if (info == "folio")                          return QObject::tr("Numéro de folio");
	else if (info == "folio-id")                       return QObject::tr("Position du folio");
	else if (info == "folio-total")                    return QObject::tr("Nombre de folio");
	else if (info == "previous-folio-num")             return QObject::tr("Numéro du folio précédent");
	else if (info == "next-folio-num")                 return QObject::tr("Numéro du folio suivant");
	else if (info == "projecttitle")                   return QObject::tr("Titre du projet");
	else if (info == "projectpath")                    return QObject::tr("Chemin du fichier du projet");
	else if (info == "projectfilename")                return QObject::tr("Nom du fichier");
	else if (info == "saveddate")                      return QObject::tr("Date d'enregistrement du fichier format local");
	else if (info == "saveddate-eu")                   return QObject::tr("Date d'enregistrement du fichier format dd-MM-yyyy");
	else if (info == "saveddate-us")                   return QObject::tr("Date d'enregistrement du fichier format yyyy-MM-dd");
	else if (info == "savedtime")                      return QObject::tr("Heure d'enregistrement du fichier");
	else if (info == "savedfilename")                  return QObject::tr("Nom du fichier enregistré");
	else if (info == "savedfilepath")                  return QObject::tr("Chemin du fichier enregistré");
	else if (info == "formula")                        return QObject::tr("Formule du label");
	else if (info == ELMT_LABEL)                       return QObject::tr("Label");
	else if (info == "plant")                          return QObject::tr("Installation");
	else if (info == "location")                       return QObject::tr("Localisation");
	else if (info == "comment")                        return QObject::tr("Commentaire");
	else if (info == "function")                       return QObject::tr("Fonction");
	else if (info == "auxiliary1")                     return QObject::tr("Bloc auxiliaire 1");
	else if (info == "auxiliary2")                     return QObject::tr("Bloc auxiliaire 2");
	else if (info == "description")                    return QObject::tr("Description textuelle");
	else if (info == "designation")                    return QObject::tr("Numéro d'article");
	else if (info == "manufacturer")                   return QObject::tr("Fabricant");
	else if (info == "manufacturer_reference")         return QObject::tr("Numéro de commande");
	else if (info == "machine_manufacturer_reference") return QObject::tr("Numéro interne");
	else if (info == "supplier")                       return QObject::tr("Fournisseur");
	else if (info == "quantity")                       return QObject::tr("Quantité");
	else if (info == "unity")                          return QObject::tr("Unité");
	else if (info == "formula")                        return QObject::tr("Formule du texte");
	else if (info == "text")                           return QObject::tr("Texte");
	else if (info == COND_FUNCTION)                    return QObject::tr("Fonction");
	else if (info == COND_TENSION_PROTOCOL)            return QObject::tr("Tension / Protocole");
	else if (info == COND_COLOR)                       return QObject::tr("Couleur du fil");
	else if (info == COND_SECTION)                     return QObject::tr("Section du fil");
	else return QString();
}
