/*
	Copyright 2006-2010 Xavier Guerrin
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
#include "qetapp.h"
#include "aboutqet.h"
#include "configdialog.h"
#include "qetdiagrameditor.h"
#include "qetelementeditor.h"
#include "elementscollectionitem.h"
#include "fileelementscollection.h"
#include "insettemplate.h"
#include "qetproject.h"
#include "qtextorientationspinboxwidget.h"
#include "recentfiles.h"
#include "qeticons.h"
#include <cstdlib>
#include <iostream>
#define QUOTE(x) STRINGIFY(x)
#define STRINGIFY(x) #x

#ifdef QET_ALLOW_OVERRIDE_CED_OPTION
QString QETApp::common_elements_dir = QString();
#endif
#ifdef QET_ALLOW_OVERRIDE_CD_OPTION
QString QETApp::config_dir = QString();
#endif
QString QETApp::lang_dir = QString();
FileElementsCollection *QETApp::common_collection = 0;
FileElementsCollection *QETApp::custom_collection = 0;
QMap<uint, QETProject *> QETApp::registered_projects_ = QMap<uint, QETProject *>();
uint QETApp::next_project_id = 0;
RecentFiles *QETApp::projects_recent_files_ = 0;
RecentFiles *QETApp::elements_recent_files_ = 0;
AboutQET *QETApp::about_dialog_ = 0;
InsetTemplate *QETApp::default_inset_template_ = 0;

/**
	Constructeur
	@param argc Nombre d'arguments passes a l'application
	@param argv Arguments passes a l'application
*/
QETApp::QETApp(int &argc, char **argv) :
	QETSingleApplication(argc, argv, QString("qelectrotech-" + QETApp::userName())),
	splash_screen_(0),
	non_interactive_execution_(false)
{
	parseArguments();
	initLanguage();
	QET::Icons::initIcons();
	initConfiguration();
	initStyle();
	
	if (!non_interactive_execution_ && isRunning()) {
		// envoie les arguments a l'instance deja existante
		non_interactive_execution_ = sendMessage(
			"launched-with-args: " +
			QET::joinWithSpaces(QStringList(qet_arguments_.arguments()))
		);
	}
	
	if (non_interactive_execution_) {
		std::exit(EXIT_SUCCESS);
	}
	
	initSplashScreen();
	initSystemTray();
	
	// prise en compte des messages des autres instances
	connect(this, SIGNAL(messageAvailable(QString)), this, SLOT(messageReceived(const QString&)));
	
	// nettoyage avant de quitter l'application
	connect(this, SIGNAL(aboutToQuit()), this, SLOT(cleanup()));
	
	// connexion pour le signalmapper
	connect(&signal_map, SIGNAL(mapped(QWidget *)), this, SLOT(invertMainWindowVisibility(QWidget *)));
	
	setQuitOnLastWindowClosed(false);
	connect(this, SIGNAL(lastWindowClosed()), this, SLOT(checkRemainingWindows()));
	
	// on ouvre soit les fichiers passes en parametre soit un nouvel editeur de projet
	if (qet_arguments_.files().isEmpty()) {
		setSplashScreenStep(tr("Chargement... \311diteur de sch\351mas", "splash screen caption"));
		new QETDiagramEditor();
	} else {
		setSplashScreenStep(tr("Chargement... Ouverture des fichiers", "splash screen caption"));
		openFiles(qet_arguments_);
	}
	buildSystemTrayMenu();
	splash_screen_ -> hide();
}

/// Destructeur
QETApp::~QETApp() {
	elements_recent_files_ -> save();
	projects_recent_files_ -> save();
	delete elements_recent_files_;
	delete projects_recent_files_;
	if (about_dialog_) {
		delete about_dialog_;
	}
	delete qsti;
	delete custom_collection;
	delete common_collection;
}

/**
	@return l'instance de la QETApp
*/
QETApp *QETApp::instance() {
	return(static_cast<QETApp *>(qApp));
}

/**
	Change le langage utilise par l'application.
	@param desired_language langage voulu
*/
void QETApp::setLanguage(const QString &desired_language) {
	QString languages_path = languagesPath();
	
	// charge les eventuelles traductions pour la lib Qt
	qtTranslator.load("qt_" + desired_language, languages_path);
	installTranslator(&qtTranslator);
	
	// charge les traductions pour l'application QET
	if (!qetTranslator.load("qet_" + desired_language, languages_path)) {
		// en cas d'echec, on retombe sur les chaines natives pour le francais
		if (desired_language != "fr") {
			// utilisation de la version anglaise par defaut
			qetTranslator.load("qet_en", languages_path);
		}
	}
	installTranslator(&qetTranslator);
}

/**
	Gere les evenements relatifs au QSystemTrayIcon
	@param reason un entier representant l'evenement survenu sur le systray
*/
void QETApp::systray(QSystemTrayIcon::ActivationReason reason) {
	if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
	switch(reason) {
		case QSystemTrayIcon::Context:
			// affichage du menu
			buildSystemTrayMenu();
			qsti -> contextMenu() -> show();
			break;
		case QSystemTrayIcon::DoubleClick:
		case QSystemTrayIcon::Trigger:
			// reduction ou restauration de l'application
			fetchWindowStats(diagramEditors(), elementEditors());
			if (every_editor_reduced) restoreEveryEditor(); else reduceEveryEditor();
			break;
		case QSystemTrayIcon::Unknown:
		default: // ne rien faire
		break;
	}
}

/// Reduit toutes les fenetres de l'application dans le systray
void QETApp::reduceEveryEditor() {
	reduceDiagramEditors();
	reduceElementEditors();
	every_editor_reduced = true;
}

/// Restaure toutes les fenetres de l'application dans le systray
void QETApp::restoreEveryEditor() {
	restoreDiagramEditors();
	restoreElementEditors();
	every_editor_reduced = false;
}

/// Reduit tous les editeurs de schemas dans le systray
void QETApp::reduceDiagramEditors() {
	foreach(QETDiagramEditor *e, diagramEditors()) setMainWindowVisible(e, false);
}

/// Restaure tous les editeurs de schemas dans le systray
void QETApp::restoreDiagramEditors() {
	foreach(QETDiagramEditor *e, diagramEditors()) setMainWindowVisible(e, true);
}

/// Reduit tous les editeurs d'element dans le systray
void QETApp::reduceElementEditors() {
	foreach(QETElementEditor *e, elementEditors()) setMainWindowVisible(e, false);
}

/// Restaure tous les editeurs d'element dans le systray
void QETApp::restoreElementEditors() {
	foreach(QETElementEditor *e, elementEditors()) setMainWindowVisible(e, true);
}

/// lance un nouvel editeur de schemas
void QETApp::newDiagramEditor() {
	new QETDiagramEditor();
}

/// lance un nouvel editeur d'element
void QETApp::newElementEditor() {
	new QETElementEditor();
}

/**
	@return la collection commune
*/
ElementsCollection *QETApp::commonElementsCollection() {
	if (!common_collection) {
		common_collection = new FileElementsCollection(QETApp::commonElementsDir());
		common_collection -> setProtocol("common");
	}
	return(common_collection);
}

/**
	@return la collection utilisateur
*/
ElementsCollection *QETApp::customElementsCollection() {
	if (!custom_collection) {
		custom_collection = new FileElementsCollection(QETApp::customElementsDir());
		custom_collection -> setProtocol("custom");
	}
	return(custom_collection);
}

/**
	@return la liste des collections disponibles
	Cela inclut typiquement la collection commune, la collection perso
	ainsi que les collections embarquees dans les projets.
*/
QList<ElementsCollection *> QETApp::availableCollections() {
	QList<ElementsCollection *> coll_list;
	
	// collection commune
	coll_list << commonElementsCollection();
	
	// collection perso
	coll_list << customElementsCollection();
	
	// collections embarquees
	foreach(QETProject *opened_project, registered_projects_.values()) {
		coll_list << opened_project -> embeddedCollection();
	}
	
	return(coll_list);
}

/**
	@return le nom de l'utilisateur courant
*/
QString QETApp::userName() {
#ifndef Q_OS_WIN32
	return(QString(getenv("USER")));
#else
	return(QString(getenv("USERNAME")));
#endif
}

/**
	Renvoie le dossier des elements communs, c-a-d le chemin du dossier dans
	lequel QET doit chercher les definitions XML des elements de la collection QET.
	@return Le chemin du dossier des elements communs
*/
QString QETApp::commonElementsDir() {
#ifdef QET_ALLOW_OVERRIDE_CED_OPTION
	if (common_elements_dir != QString()) return(common_elements_dir);
#endif
#ifndef QET_COMMON_COLLECTION_PATH
	// en l'absence d'option de compilation, on utilise le dossier elements, situe a cote du binaire executable
	return(QCoreApplication::applicationDirPath() + "/elements/");
#else
	#ifndef QET_COMMON_COLLECTION_PATH_RELATIVE_TO_BINARY_PATH
		// l'option de compilation represente un chemin absolu ou relatif classique
		return(QUOTE(QET_COMMON_COLLECTION_PATH));
	#else
		// l'option de compilation represente un chemin relatif au dossier contenant le binaire executable
		return(QCoreApplication::applicationDirPath() + "/" + QUOTE(QET_COMMON_COLLECTION_PATH));
	#endif
#endif
}

/**
	Renvoie le dossier des elements de l'utilisateur, c-a-d le chemin du dossier
	dans lequel QET chercher les definitions XML des elements propres a
	l'utilisateur.
	@return Le chemin du dossier des elements persos
*/
QString QETApp::customElementsDir() {
	return(configDir() + "elements/");
}

/**
	Renvoie le dossier de configuration de QET, c-a-d le chemin du dossier dans
	lequel QET lira les informations de configuration et de personnalisation
	propres a l'utilisateur courant. Ce dossier est generalement
	C:\\Documents And Settings\\utilisateur\\Application Data\\qet sous Windows et
	~/.qet sous les systemes type UNIX.
	@return Le chemin du dossier de configuration de QElectroTech
*/
QString QETApp::configDir() {
#ifdef QET_ALLOW_OVERRIDE_CD_OPTION
	if (config_dir != QString()) return(config_dir);
#endif
#ifdef Q_OS_WIN32
	// recupere l'emplacement du dossier Application Data
	char *app_data_env = getenv("APPDATA");
	QString app_data_str(app_data_env);
	delete app_data_env;
	if (app_data_str.isEmpty()) {
		app_data_str = QDir::homePath() + "/Application Data";
	}
	return(app_data_str + "/qet/");
#else
	return(QDir::homePath() + "/.qet/");
#endif
}

/**
	Permet de connaitre le chemin absolu du fichier *.elmt correspondant a un
	chemin symbolique (du type custom://outils_pervers/sado_maso/contact_bizarre)
	@param sym_path Chaine de caracteres representant le chemin absolu du fichier
	@return Une chaine de caracteres vide en cas d'erreur ou le chemin absolu du
	fichier *.elmt.
*/
QString QETApp::realPath(const QString &sym_path) {
	QString directory;
	if (sym_path.startsWith("common://")) {
		directory = commonElementsDir();
	} else if (sym_path.startsWith("custom://")) {
		directory = customElementsDir();
	} else return(QString());
	return(directory + QDir::toNativeSeparators(sym_path.right(sym_path.length() - 9)));
}

/**
	Construit le chemin symbolique (du type custom://outils_pervers/sado_maso/
	contact_bizarre) correspondant a un fichier.
	@param real_path Chaine de caracteres representant le chemin symbolique du fichier
	@return Une chaine de caracteres vide en cas d'erreur ou le chemin
	symbolique designant l'element.
*/
QString QETApp::symbolicPath(const QString &real_path) {
	// recupere les dossier common et custom
	QString commond = commonElementsDir();
	QString customd = customElementsDir();
	QString chemin;
	// analyse le chemin de fichier passe en parametre
	if (real_path.startsWith(commond)) {
		chemin = "common://" + real_path.right(real_path.length() - commond.length());
	} else if (real_path.startsWith(customd)) {
		chemin = "custom://" + real_path.right(real_path.length() - customd.length());
	} else chemin = QString();
	return(chemin);
}

/**
	@param filepath Un chemin de fichier
	Note : si filepath est une chaine vide, cette methode retourne 0.
	@return le QETDiagramEditor editant le fichier filepath, ou 0 si ce fichier
	n'est pas edite par l'application.
*/
QETDiagramEditor *QETApp::diagramEditorForFile(const QString &filepath) {
	if (filepath.isEmpty()) return(0);
	
	QETApp *qet_app(QETApp::instance());
	foreach (QETDiagramEditor *diagram_editor, qet_app -> diagramEditors()) {
		if (diagram_editor -> viewForFile(filepath)) {
			return(diagram_editor);
		}
	}
	
	return(0);
}

#ifdef QET_ALLOW_OVERRIDE_CED_OPTION
/**
	Redefinit le chemin du dossier des elements communs
	@param new_ced Nouveau chemin du dossier des elements communs
*/
void QETApp::overrideCommonElementsDir(const QString &new_ced) {
	QFileInfo new_ced_info(new_ced);
	if (new_ced_info.isDir()) {
		common_elements_dir = new_ced_info.absoluteFilePath();
		if (!common_elements_dir.endsWith("/")) common_elements_dir += "/";
	}
}
#endif

#ifdef QET_ALLOW_OVERRIDE_CD_OPTION
/**
	Redefinit le chemin du dossier de configuration
	@param new_cd Nouveau chemin du dossier de configuration
*/
void QETApp::overrideConfigDir(const QString &new_cd) {
	QFileInfo new_cd_info(new_cd);
	if (new_cd_info.isDir()) {
		config_dir = new_cd_info.absoluteFilePath();
		if (!config_dir.endsWith("/")) config_dir += "/";
	}
}
#endif

/**
	Redefinit le chemin du dossier contenant les fichiers de langue
	@param new_ld Nouveau chemin du dossier contenant les fichiers de langue
*/
void QETApp::overrideLangDir(const QString &new_ld) {
	QFileInfo new_ld_info(new_ld);
	if (new_ld_info.isDir()) {
		lang_dir = new_ld_info.absoluteFilePath();
		if (!lang_dir.endsWith("/")) lang_dir += "/";
	}
}

/**
	@return Le chemin du dossier contenant les fichiers de langue
*/
QString QETApp::languagesPath() {
	if (!lang_dir.isEmpty()) {
		return(lang_dir);
	} else {
#ifndef QET_LANG_PATH
	// en l'absence d'option de compilation, on utilise le dossier lang, situe a cote du binaire executable
	return(QCoreApplication::applicationDirPath() + "/lang/");
#else
	#ifndef QET_LANG_PATH_RELATIVE_TO_BINARY_PATH
		// l'option de compilation represente un chemin absolu ou relatif classique
		return(QUOTE(QET_LANG_PATH));
	#else
		// l'option de compilation represente un chemin relatif au dossier contenant le binaire executable
		return(QCoreApplication::applicationDirPath() + "/" + QUOTE(QET_LANG_PATH));
	#endif
#endif
	}
}

/**
	Ferme tous les editeurs
	@return true si l'utilisateur a accepte toutes les fermetures, false sinon
*/
bool QETApp::closeEveryEditor() {
	// s'assure que toutes les fenetres soient visibles avant de quitter
	restoreEveryEditor();
	foreach(QETProject *project, registered_projects_) {
		project -> close();
	}
	bool every_window_closed = true;
	foreach(QETDiagramEditor *e, diagramEditors()) {
		every_window_closed = every_window_closed && e -> close();
	}
	foreach(QETElementEditor *e, elementEditors()) {
		every_window_closed = every_window_closed && e -> close();
	}
	return(every_window_closed);
}

/**
	@param size taille voulue - si aucune taille n'est specifiee, la valeur
	specifiee dans la configuration (diagramsize) est utilisee. La valeur par
	defaut est 9.
	@return la police a utiliser pour rendre les textes sur les schemas
	La famille "Sans Serif" est utilisee par defaut mais peut etre surchargee
	dans la configuration (diagramfont).
*/
QFont QETApp::diagramTextsFont(int size) {
	// acces a la configuration de l'application
	QSettings &qet_settings = QETApp::settings();
	
	// police a utiliser pour le rendu de texte
	QString diagram_texts_family = qet_settings.value("diagramfont", "Sans Serif").toString();
	int     diagram_texts_size   = qet_settings.value("diagramsize", 9).toInt();
	
	if (size != -1) {
		diagram_texts_size = size;
	}
	QFont diagram_texts_font = QFont(diagram_texts_family, diagram_texts_size);
	if (diagram_texts_size <= 4) {
		diagram_texts_font.setWeight(QFont::Light);
	}
	return(diagram_texts_font);
}

/**
	@return les editeurs de schemas
*/
QList<QETDiagramEditor *> QETApp::diagramEditors() {
	return(static_cast<QETApp *>(qApp) -> detectDiagramEditors());
}

/**
	@return les editeurs d'elements
*/
QList<QETElementEditor *> QETApp::elementEditors() {
	return(static_cast<QETApp *>(qApp) -> detectElementEditors());
}

/**
	Instancie un QTextOrientationSpinBoxWidget et configure :
	  * sa police de caracteres
	  * ses chaines de caracteres
	A noter que la suppression du widget ainsi alloue est a la charge de
	l'appelant.
	@return un QTextOrientationSpinBoxWidget adapte pour une utilisation
	"directe" dans QET.
	@see QTextOrientationSpinBoxWidget
*/
QTextOrientationSpinBoxWidget *QETApp::createTextOrientationSpinBoxWidget() {
	QTextOrientationSpinBoxWidget *widget = new QTextOrientationSpinBoxWidget();
	widget -> orientationWidget() -> setFont(QETApp::diagramTextsFont());
	widget -> orientationWidget() -> setUsableTexts(QList<QString>()
		<< QETApp::tr("Q",            "Single-letter example text - translate length, not meaning")
		<< QETApp::tr("QET",          "Small example text - translate length, not meaning")
		<< QETApp::tr("Schema",       "Normal example text - translate length, not meaning")
		<< QETApp::tr("Electrique",   "Normal example text - translate length, not meaning")
		<< QETApp::tr("QElectroTech", "Long example text - translate length, not meaning")
	);
	return(widget);
}

/**
	@return the default inset template for diagrams
*/
InsetTemplate *QETApp::defaultInsetTemplate() {
	if (!QETApp::default_inset_template_) {
		InsetTemplate *inset_template = new InsetTemplate(QETApp::instance());
		if (inset_template -> loadFromXmlFile(":/insets/default.inset")) {
			QETApp::default_inset_template_ = inset_template;
		}
	}
	return(default_inset_template_);
}


/**
	@param project un projet
	@return les editeurs d'elements editant un element appartenant au projet
	project
*/
QList<QETElementEditor *> QETApp::elementEditors(QETProject *project) {
	QList<QETElementEditor *> editors;
	if (!project) return(editors);
	
	// pour chaque editeur d'element...
	foreach(QETElementEditor *elmt_editor, elementEditors()) {
		// on recupere l'emplacement de l'element qu'il edite
		ElementsLocation elmt_editor_loc(elmt_editor -> location());
		
		// il se peut que l'editeur edite un element non enregistre ou un fichier
		if (elmt_editor_loc.isNull()) continue;
		
		if (elmt_editor_loc.project() == project) {
			editors << elmt_editor;
		}
	}
	
	return(editors);
}

/**
	Nettoie certaines choses avant que l'application ne quitte
*/
void QETApp::cleanup() {
	qsti -> hide();
}

/// @return les editeurs de schemas ouverts
QList<QETDiagramEditor *> QETApp::detectDiagramEditors() const {
	QList<QETDiagramEditor *> diagram_editors;
	foreach(QWidget *qw, topLevelWidgets()) {
		if (!qw -> isWindow()) continue;
		if (QETDiagramEditor *de = qobject_cast<QETDiagramEditor *>(qw)) {
			diagram_editors << de;
		}
	}
	return(diagram_editors);
}

/// @return les editeurs d'elements ouverts
QList<QETElementEditor *> QETApp::detectElementEditors() const {
	QList<QETElementEditor *> element_editors;
	foreach(QWidget *qw, topLevelWidgets()) {
		if (!qw -> isWindow()) continue;
		if (QETElementEditor *ee = qobject_cast<QETElementEditor *>(qw)) {
			element_editors << ee;
		}
	}
	return(element_editors);
}

/**
	@return La liste des fichiers recents pour les projets
*/
RecentFiles *QETApp::projectsRecentFiles() {
	return(projects_recent_files_);
}

/**
	@return La liste des fichiers recents pour les elements
*/
RecentFiles *QETApp::elementsRecentFiles() {
	return(elements_recent_files_);
}

/**
	Affiche ou cache une fenetre (editeurs de schemas / editeurs d'elements)
	@param window fenetre a afficher / cacher
	@param visible true pour affiche la fenetre, false sinon
*/
void QETApp::setMainWindowVisible(QMainWindow *window, bool visible) {
	if (window -> isVisible() == visible) return;
	if (!visible) {
		window_geometries.insert(window, window -> saveGeometry());
		window_states.insert(window, window -> saveState());
		window -> hide();
		// cache aussi les toolbars et les docks
		foreach (QWidget *qw, floatingToolbarsAndDocksForMainWindow(window)) {
			qw -> hide();
		}
	} else {
		window -> show();
#ifndef Q_OS_WIN32
		window -> restoreGeometry(window_geometries[window]);
#endif
		window -> restoreState(window_states[window]);
	}
}

/**
	Affiche une fenetre (editeurs de schemas / editeurs d'elements) si
	celle-ci est cachee ou la cache si elle est affichee.
	@param window fenetre a afficher / cacher
*/
void QETApp::invertMainWindowVisibility(QWidget *window) {
	if (QMainWindow *w = qobject_cast<QMainWindow *>(window)) setMainWindowVisible(w, !w -> isVisible());
}

/**
	Change la palette de l'application
	@param use true pour utiliser les couleurs du systeme, false pour utiliser celles du theme en cours
*/
void QETApp::useSystemPalette(bool use) {
	if (use) {
		setPalette(initial_palette_);
	} else {
		setPalette(style() -> standardPalette());
	}
	
	// reapplique les feuilles de style
	setStyleSheet(
		"QAbstractScrollArea#mdiarea {"
		"	background-color:white;"
		"	background-image: url(':/ico/mdiarea_bg.png');"
		"	background-repeat: no-repeat;"
		"	background-position: center middle;"
		"}"
	);
}

/**
	Demande la fermeture de toutes les fenetres ; si l'utilisateur les accepte,
	l'application quitte
*/
void QETApp::quitQET() {
	if (closeEveryEditor()) {
		quit();
	}
}

/**
	Verifie s'il reste des fenetres (cachees ou non) et quitte s'il n'en reste
	plus.
*/
void QETApp::checkRemainingWindows() {
	/* petite bidouille : le slot se rappelle apres 500 ms d'attente
	afin de compenser le fait que certaines fenetres peuvent encore
	paraitre vivantes alors qu'elles viennent d'etre fermees
	*/
	static bool sleep = true;
	if (sleep) {
		QTimer::singleShot(500, this, SLOT(checkRemainingWindows()));
	} else {
		if (!diagramEditors().count() && !elementEditors().count()) {
			quit();
		}
	}
	sleep = !sleep;
}

/**
	Gere les messages recus
	@param message Message recu
*/
void QETApp::messageReceived(const QString &message) {
	if (message.startsWith("launched-with-args: ")) {
		QString my_message(message.mid(20));
		// les arguments sont separes par des espaces non echappes
		QStringList args_list = QET::splitWithSpaces(my_message);
		openFiles(QETArguments(args_list));
	}
}

/**
	Ouvre les fichiers passes en arguments
	@param args Objet contenant des arguments ; les fichiers 
	@see openProjectFiles openElementFiles
*/
void QETApp::openFiles(const QETArguments &args) {
	openProjectFiles(args.projectFiles());
	openElementFiles(args.elementFiles());
}

/**
	Ouvre une liste de fichiers.
	Les fichiers sont ouverts dans le premier editeur de schemas visible venu.
	Sinon, le premier editeur de schemas existant venu devient visible et est
	utilise. S'il n'y a aucun editeur de schemas ouvert, un nouveau est cree et
	utilise.
	@param files_list Fichiers a ouvrir
*/
void QETApp::openProjectFiles(const QStringList &files_list) {
	if (files_list.isEmpty()) return;
	
	// liste des editeurs de schema ouverts
	QList<QETDiagramEditor *> diagrams_editors = diagramEditors();
	
	// s'il y a des editeur de schemas ouvert, on cherche ceux qui sont visibles
	if (diagrams_editors.count()) {
		QList<QETDiagramEditor *> visible_diagrams_editors;
		foreach(QETDiagramEditor *de, diagrams_editors) {
			if (de -> isVisible()) visible_diagrams_editors << de;
		}
		
		// on choisit soit le premier visible soit le premier tout court
		QETDiagramEditor *de_open;
		if (visible_diagrams_editors.count()) {
			de_open = visible_diagrams_editors.first();
		} else {
			de_open = diagrams_editors.first();
			de_open -> setVisible(true);
		}
		
		// ouvre les fichiers dans l'editeur ainsi choisi
		foreach(QString file, files_list) {
			de_open -> openAndAddProject(file);
		}
	} else {
		// cree un nouvel editeur qui ouvrira les fichiers
		new QETDiagramEditor(files_list);
	}
}

/**
	Ouvre les fichiers elements passes en parametre. Si un element est deja
	ouvert, la fentre qui l'edite est activee.
	@param files_list Fichiers a ouvrir
*/
void QETApp::openElementFiles(const QStringList &files_list) {
	if (files_list.isEmpty()) return;
	
	// evite autant que possible les doublons dans la liste fournie
	QSet<QString> files_set;
	foreach(QString file, files_list) {
		QString canonical_filepath = QFileInfo(file).canonicalFilePath();
		if (!canonical_filepath.isEmpty()) files_set << canonical_filepath;
	}
	// a ce stade, tous les fichiers dans le Set existent et sont a priori differents
	if (files_set.isEmpty()) return;
	
	// liste des editeurs d'element ouverts
	QList<QETElementEditor *> element_editors = elementEditors();
	
	// on traite les fichiers a la queue leu leu...
	foreach(QString element_file, files_set) {
		bool already_opened_in_existing_element_editor = false;
		foreach(QETElementEditor *element_editor, element_editors) {
			if (element_editor -> isEditing(element_file)) {
				// ce fichier est deja ouvert dans un editeur
				already_opened_in_existing_element_editor = true;
				element_editor -> setVisible(true);
				element_editor -> raise();
				element_editor -> activateWindow();
				break;
			}
		}
		if (!already_opened_in_existing_element_editor) {
			// ce fichier n'est ouvert dans aucun editeur
			QETElementEditor *element_editor = new QETElementEditor();
			element_editor -> fromFile(element_file);
		}
	}
}

/**
	Ouvre les elements dont l'emplacement est passe en parametre. Si un element
	est deja ouvert, la fentre qui l'edite est activee.
	@param locations_list Emplacements a ouvrir
*/
void QETApp::openElementLocations(const QList<ElementsLocation> &locations_list) {
	if (locations_list.isEmpty()) return;
	
	// liste des editeurs d'element ouverts
	QList<QETElementEditor *> element_editors = elementEditors();
	
	// on traite les emplacements  a la queue leu leu...
	foreach(ElementsLocation element_location, locations_list) {
		bool already_opened_in_existing_element_editor = false;
		foreach(QETElementEditor *element_editor, element_editors) {
			if (element_editor -> isEditing(element_location)) {
				// cet emplacement est deja ouvert dans un editeur
				already_opened_in_existing_element_editor = true;
				element_editor -> setVisible(true);
				element_editor -> raise();
				element_editor -> activateWindow();
				break;
			}
		}
		if (!already_opened_in_existing_element_editor) {
			// cet emplacement n'est ouvert dans aucun editeur
			QETElementEditor *element_editor = new QETElementEditor();
			element_editor -> fromLocation(element_location);
		}
	}
}

/**
	Permet a l'utilisateur de configurer QET en lancant un dialogue approprie.
	@see ConfigDialog
*/
void QETApp::configureQET() {
	// determine le widget parent a utiliser pour le dialogue
	QWidget *parent_widget = activeWindow();
	
	// cree le dialogue
	ConfigDialog cd;
	
	// associe le dialogue a un eventuel widget parent
	if (parent_widget) {
#ifdef Q_WS_MAC
		cd.setWindowFlags(Qt::Sheet);
#endif
		cd.setParent(parent_widget, cd.windowFlags());
	}
	
	// affiche le dialogue puis evite de le lier a un quelconque widget parent
	cd.exec();
	cd.setParent(0, cd.windowFlags());
}

/**
	Dialogue "A propos de QElectroTech"
	Le dialogue en question est cree lors du premier appel de cette fonction.
	En consequence, sa premiere apparition n'est pas immediate. Par la suite,
	le dialogue n'a pas a etre recree et il apparait instantanement. Il est
	detruit en meme temps que l'application.
*/
void QETApp::aboutQET() {
	// determine le widget parent a utiliser pour le dialogue
	QWidget *parent_widget = activeWindow();
	
	// cree le dialogue si cela n'a pas deja ete fait
	if (!about_dialog_) {
		about_dialog_ = new AboutQET();
	}
	
	// associe le dialogue a un eventuel widget parent
	if (parent_widget) {
#ifdef Q_WS_MAC
		about_dialog_ -> setWindowFlags(Qt::Sheet);
#endif
		about_dialog_ -> setParent(parent_widget, about_dialog_ -> windowFlags());
	}
	
	// affiche le dialogue puis evite de le lier a un quelconque widget parent
	about_dialog_ -> exec();
	about_dialog_ -> setParent(0, about_dialog_ -> windowFlags());
}

/**
	@param window fenetre dont il faut trouver les barres d'outils et dock flottants
	@return les barres d'outils et dock flottants de la fenetre
*/
QList<QWidget *> QETApp::floatingToolbarsAndDocksForMainWindow(QMainWindow *window) const {
	QList<QWidget *> widgets;
	foreach(QWidget *qw, topLevelWidgets()) {
		if (!qw -> isWindow()) continue;
		if (qobject_cast<QToolBar *>(qw) || qobject_cast<QDockWidget *>(qw)) {
			if (qw -> parent() == window) widgets << qw;
		}
	}
	return(widgets);
}

/**
	Parse les arguments suivants :
	  * --common-elements-dir=
	  * --config-dir
	  * --help
	  * --version
	  * -v
	  * --license
	Les autres arguments sont normalement des chemins de fichiers.
	S'ils existent, ils sont juste memorises dans l'attribut arguments_files_.
	Sinon, ils sont memorises dans l'attribut arguments_options_.
*/
void QETApp::parseArguments() {
	// recupere les arguments
	QList<QString> arguments_list(arguments());
	
	// enleve le premier argument : il s'agit du fichier binaire
	arguments_list.takeFirst();
	
	// analyse les arguments
	qet_arguments_ = QETArguments(arguments_list);
	
#ifdef QET_ALLOW_OVERRIDE_CED_OPTION
	if (qet_arguments_.commonElementsDirSpecified()) {
		overrideCommonElementsDir(qet_arguments_.commonElementsDir());
	}
#endif
#ifdef QET_ALLOW_OVERRIDE_CD_OPTION
	if (qet_arguments_.configDirSpecified()) {
		overrideConfigDir(qet_arguments_.configDir());
	}
#endif
	
	if (qet_arguments_.langDirSpecified()) {
		overrideLangDir(qet_arguments_.langDir());
	}
	
	if (qet_arguments_.printLicenseRequested()) {
		printLicense();
		non_interactive_execution_ = true;
	}
	if (qet_arguments_.printHelpRequested()) {
		printHelp();
		non_interactive_execution_ = true;
	}
	if (qet_arguments_.printVersionRequested()) {
		printVersion();
		non_interactive_execution_ = true;
	}
}

/**
	Initialise le splash screen si et seulement si l'execution est interactive.
	Autrement, l'attribut splash_screen_ vaut 0.
*/
void QETApp::initSplashScreen() {
	if (non_interactive_execution_) return;
	splash_screen_ = new QSplashScreen(QPixmap(":/ico/splash.png"));
	splash_screen_ -> show();
	setSplashScreenStep(tr("Chargement...", "splash screen caption"));
}

/**
	Change le texte du splash screen et prend en compte les evenements.
	Si l'application s'execute de facon non interactive, cette methode ne fait
	rien.
*/
void QETApp::setSplashScreenStep(const QString &message) {
	if (!splash_screen_) return;
	if (!message.isEmpty()) {
		splash_screen_ -> showMessage(message, Qt::AlignBottom | Qt::AlignLeft);
	}
	processEvents();
}

/**
	Determine et applique le langage a utiliser pour l'application
*/
void QETApp::initLanguage() {
	// selectionne le langage du systeme
	QString system_language = QLocale::system().name().left(2);
	setLanguage(system_language);
}

/**
	Met en place tout ce qui concerne le style graphique de l'application
*/
void QETApp::initStyle() {
	initial_palette_ = palette();
	
	// lorsque le style Plastique est active, on le remplace par une version amelioree
	if (qobject_cast<QPlastiqueStyle *>(style())) {
		setStyle(new QETStyle());
	}
	
	// applique ou non les couleurs de l'environnement
	useSystemPalette(settings().value("usesystemcolors", true).toBool());
}

/**
	Lit et prend en compte la configuration de l'application.
	Cette methode creera, si necessaire :
	  * le dossier de configuration
	  * le dossier de la collection perso
*/
void QETApp::initConfiguration() {
	// cree les dossiers de configuration si necessaire
	QDir config_dir(QETApp::configDir());
	if (!config_dir.exists()) config_dir.mkpath(QETApp::configDir());
	
	QDir custom_elements_dir(QETApp::customElementsDir());
	if (!custom_elements_dir.exists()) custom_elements_dir.mkpath(QETApp::customElementsDir());
	
	// lit le fichier de configuration
	qet_settings = new QSettings(configDir() + "qelectrotech.conf", QSettings::IniFormat, this);
	
	// fichiers recents
	// note : les icones doivent etre initialisees avant ces instructions (qui creent des menus en interne)
	projects_recent_files_ = new RecentFiles("projects");
	projects_recent_files_ -> setIconForFiles(QET::Icons::ProjectFile);
	elements_recent_files_ = new RecentFiles("elements");
	elements_recent_files_ -> setIconForFiles(QET::Icons::Element);
}

/**
	Construit l'icone dans le systray et son menu
*/
void QETApp::initSystemTray() {
	setSplashScreenStep(tr("Chargement... ic\364ne du systray", "splash screen caption"));
	// initialisation des menus de l'icone dans le systray
	menu_systray = new QMenu(tr("QElectroTech", "systray menu title"));
	
	quitter_qet       = new QAction(QET::Icons::ApplicationExit,       tr("&Quitter"),                                        this);
	reduce_appli      = new QAction(QET::Icons::Hide,    tr("&Masquer"),                                        this);
	restore_appli     = new QAction(QET::Icons::Restore,  tr("&Restaurer"),                                      this);
	reduce_diagrams   = new QAction(QET::Icons::Hide,    tr("&Masquer tous les \351diteurs de sch\351ma"),      this);
	restore_diagrams  = new QAction(QET::Icons::Restore,  tr("&Restaurer tous les \351diteurs de sch\351ma"),    this);
	reduce_elements   = new QAction(QET::Icons::Hide,    tr("&Masquer tous les \351diteurs d'\351l\351ment"),   this);
	restore_elements  = new QAction(QET::Icons::Restore,  tr("&Restaurer tous les \351diteurs d'\351l\351ment"), this);
	new_diagram       = new QAction(QET::Icons::WindowNew, tr("&Nouvel \351diteur de sch\351ma"),                 this);
	new_element       = new QAction(QET::Icons::WindowNew, tr("&Nouvel \351diteur d'\351l\351ment"),              this);
	
	quitter_qet   -> setStatusTip(tr("Ferme l'application QElectroTech"));
	reduce_appli  -> setToolTip(tr("R\351duire QElectroTech dans le systray"));
	restore_appli -> setToolTip(tr("Restaurer QElectroTech"));
	
	connect(quitter_qet,      SIGNAL(triggered()), this, SLOT(quitQET()));
	connect(reduce_appli,     SIGNAL(triggered()), this, SLOT(reduceEveryEditor()));
	connect(restore_appli,    SIGNAL(triggered()), this, SLOT(restoreEveryEditor()));
	connect(reduce_diagrams,  SIGNAL(triggered()), this, SLOT(reduceDiagramEditors()));
	connect(restore_diagrams, SIGNAL(triggered()), this, SLOT(restoreDiagramEditors()));
	connect(reduce_elements,  SIGNAL(triggered()), this, SLOT(reduceElementEditors()));
	connect(restore_elements, SIGNAL(triggered()), this, SLOT(restoreElementEditors()));
	connect(new_diagram,      SIGNAL(triggered()), this, SLOT(newDiagramEditor()));
	connect(new_element,      SIGNAL(triggered()), this, SLOT(newElementEditor()));
	
	// initialisation de l'icone du systray
	qsti = new QSystemTrayIcon(QET::Icons::QETLogo, this);
	qsti -> setToolTip(tr("QElectroTech", "systray icon tooltip"));
	connect(qsti, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(systray(QSystemTrayIcon::ActivationReason)));
	qsti -> setContextMenu(menu_systray);
	qsti -> show();
}

/// construit le menu de l'icone dans le systray
void QETApp::buildSystemTrayMenu() {
	menu_systray -> clear();
	
	// recupere les editeurs
	QList<QETDiagramEditor *> diagrams = diagramEditors();
	QList<QETElementEditor *> elements = elementEditors();
	fetchWindowStats(diagrams, elements);
	
	// ajoute le bouton reduire / restaurer au menu
	menu_systray -> addAction(every_editor_reduced ? restore_appli : reduce_appli);
	
	// ajoute les editeurs de schemas dans un sous-menu
	QMenu *diagrams_submenu = menu_systray -> addMenu(tr("\311diteurs de sch\351mas"));
	diagrams_submenu -> addAction(reduce_diagrams);
	diagrams_submenu -> addAction(restore_diagrams);
	diagrams_submenu -> addAction(new_diagram);
	reduce_diagrams -> setEnabled(!diagrams.isEmpty() && !every_diagram_reduced);
	restore_diagrams -> setEnabled(!diagrams.isEmpty() && !every_diagram_visible);
	diagrams_submenu -> addSeparator();
	foreach(QETDiagramEditor *diagram, diagrams) {
		QAction *current_diagram_menu = diagrams_submenu -> addAction(diagram -> windowTitle());
		current_diagram_menu -> setCheckable(true);
		current_diagram_menu -> setChecked(diagram -> isVisible());
		connect(current_diagram_menu, SIGNAL(triggered()), &signal_map, SLOT(map()));
		signal_map.setMapping(current_diagram_menu, diagram);
	}
	
	// ajoute les editeurs d'elements au menu
	QMenu *elements_submenu = menu_systray -> addMenu(tr("\311diteurs d'\351l\351ment"));
	elements_submenu -> addAction(reduce_elements);
	elements_submenu -> addAction(restore_elements);
	elements_submenu -> addAction(new_element);
	reduce_elements -> setEnabled(!elements.isEmpty() && !every_element_reduced);
	restore_elements -> setEnabled(!elements.isEmpty() && !every_element_visible);
	elements_submenu -> addSeparator();
	foreach(QETElementEditor *element, elements) {
		QAction *current_element_menu = elements_submenu -> addAction(element -> windowTitle());
		current_element_menu -> setCheckable(true);
		current_element_menu -> setChecked(element -> isVisible());
		connect(current_element_menu, SIGNAL(triggered()), &signal_map, SLOT(map()));
		signal_map.setMapping(current_element_menu, element);
	}
	
	// ajoute le bouton quitter au menu
	menu_systray -> addSeparator();
	menu_systray -> addAction(quitter_qet);
}

/// Met a jour les booleens concernant l'etat des fenetres
void QETApp::fetchWindowStats(const QList<QETDiagramEditor *> &diagrams, const QList<QETElementEditor *> &elements) {
	// compte le nombre de schemas visibles
	int visible_diagrams = 0;
	foreach(QMainWindow *w, diagrams) if (w -> isVisible()) ++ visible_diagrams;
	every_diagram_reduced = !visible_diagrams;
	every_diagram_visible = visible_diagrams == diagrams.count();
	
	// compte le nombre de schemas visibles
	int visible_elements = 0;
	foreach(QMainWindow *w, elements) if (w -> isVisible()) ++ visible_elements;
	every_element_reduced = !visible_elements;
	every_element_visible = visible_elements == elements.count();
	
	// determine si tous les elements sont reduits
	every_editor_reduced = every_element_reduced && every_diagram_reduced;
}

#ifdef Q_OS_DARWIN
/**
	Gere les evenements, en particulier l'evenement FileOpen sous MacOs.
	@param e Evenement a gerer
*/
bool QETApp::event(QEvent *e) {
	// gere l'ouverture de fichiers (sous MacOs)
	if (e -> type() == QEvent::FileOpen) {
		// nom du fichier a ouvrir
		QString filename = static_cast<QFileOpenEvent *>(e) -> file();
		openFiles(QStringList() << filename);
		return(true);
	} else {
		return(QApplication::event(e));
	}
}
#endif

/**
	Affiche l'aide et l'usage sur la sortie standard
*/
void QETApp::printHelp() {
	QString help(
		tr("Usage : ") + QFileInfo(applicationFilePath()).fileName() + tr(" [options] [fichier]...\n\n") +
		tr("QElectroTech, une application de r\351alisation de sch\351mas \351lectriques.\n\n"
		"Options disponibles : \n"
		"  --help                        Afficher l'aide sur les options\n"
		"  -v, --version                 Afficher la version\n"
		"  --license                     Afficher la licence\n")
#ifdef QET_ALLOW_OVERRIDE_CED_OPTION
		+ tr("  --common-elements-dir=DIR     Definir le dossier de la collection d'elements\n")
#endif
#ifdef QET_ALLOW_OVERRIDE_CD_OPTION
		+ tr("  --config-dir=DIR              Definir le dossier de configuration\n")
#endif
		+ tr("  --lang-dir=DIR                Definir le dossier contenant les fichiers de langue\n")
	);
	std::cout << qPrintable(help) << std::endl;
}

/**
	Affiche la version sur la sortie standard
*/
void QETApp::printVersion() {
	std::cout << qPrintable(QET::displayedVersion) << std::endl;
}

/**
	Affiche la licence sur la sortie standard
*/
void QETApp::printLicense() {
	std::cout << qPrintable(QET::license()) << std::endl;
}

/// Constructeur
QETStyle::QETStyle() : QPlastiqueStyle() {
}

/// Destructeur
QETStyle::~QETStyle() {
}

/// Gere les parametres de style
int QETStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returndata) const {
	if (hint == QStyle::SH_DialogButtonBox_ButtonsHaveIcons) {
		return(int(true));
	} else {
		return(QPlastiqueStyle::styleHint(hint, option, widget, returndata));
	}
}

/// Gere les icones standard
QIcon QETStyle::standardIconImplementation(StandardPixmap standardIcon, const QStyleOption *option, const QWidget* widget) const {
	switch(standardIcon) {
		case QStyle::SP_DialogSaveButton:
			return(QET::Icons::DocumentSave);
		case QStyle::SP_DialogOpenButton:
			return(QET::Icons::DocumentOpen);
		case QStyle::SP_DialogCancelButton:
			return(QET::Icons::DialogCancel);
		case QStyle::SP_DialogOkButton:
		case QStyle::SP_DialogApplyButton:
			return(QET::Icons::DialogOk);
		case QStyle::SP_DialogCloseButton:
			return(QET::Icons::DocumentClose);
		case QStyle::SP_DialogYesButton:
			return(QET::Icons::Allowed);
		case QStyle::SP_DialogNoButton:
			return(QET::Icons::Forbidden);
		case QStyle::SP_DialogResetButton:
			return(QET::Icons::EditUndo);
		case QStyle::SP_DialogHelpButton:
		case QStyle::SP_DialogDiscardButton:
			return(QIcon());
		default:
			return(QPlastiqueStyle::standardIconImplementation(standardIcon, option, widget));
	}
}

/// @return une reference vers les parametres de QElectroTEch
QSettings &QETApp::settings() {
	return(*(instance() -> qet_settings));
}

/**
	@param location adresse virtuelle d'un item (collection, categorie, element, ...)
	@param prefer_collections true pour renvoyer la collection lorsque le
	chemin correspond aussi bien a une collection qu'a sa categorie racine
	@return l'item correspondant a l'adresse virtuelle path, ou 0 si celui-ci n'a pas ete trouve
*/
ElementsCollectionItem *QETApp::collectionItem(const ElementsLocation &location, bool prefer_collections) {
	if (QETProject *target_project = location.project()) {
		return(target_project -> embeddedCollection() -> item(location.path(), prefer_collections));
	} else {
		QString path(location.path());
		if (path.startsWith("common://")) {
			return(common_collection -> item(path, prefer_collections));
		} else if (path.startsWith("custom://")) {
			return(custom_collection -> item(path, prefer_collections));
		}
	}
	return(0);
}

/**
	@param location adresse virtuelle de la categorie a creer
	@return la categorie creee, ou 0 en cas d'echec
*/
ElementsCategory *QETApp::createCategory(const ElementsLocation &location) {
	if (QETProject *target_project = location.project()) {
		return(target_project -> embeddedCollection() -> createCategory(location.path()));
	} else {
		QString path(location.path());
		if (path.startsWith("common://")) {
			return(common_collection -> createCategory(path));
		} else if (path.startsWith("custom://")) {
			return(custom_collection -> createCategory(path));
		}
	}
	return(0);
}

/**
	@param location adresse virtuelle de l'element a creer
	@return l'element cree, ou 0 en cas d'echec
*/
ElementDefinition *QETApp::createElement(const ElementsLocation &location) {
	if (QETProject *target_project = location.project()) {
		return(target_project -> embeddedCollection() -> createElement(location.path()));
	} else {
		QString path(location.path());
		if (path.startsWith("common://")) {
			return(common_collection -> createElement(path));
		} else if (path.startsWith("custom://")) {
			return(custom_collection -> createElement(path));
		}
	}
	return(0);
}

/**
	@return la liste des projets avec leurs ids associes
*/
QMap<uint, QETProject *> QETApp::registeredProjects() {
	return(registered_projects_);
}

/**
	@param project Projet a enregistrer aupres de l'application
	@return true si le projet a pu etre enregistre, false sinon
	L'echec de l'enregistrement d'un projet signifie generalement qu'il est deja enregistre.
*/
bool QETApp::registerProject(QETProject *project) {
	// le projet doit sembler valide
	if (!project) return(false);
	
	// si le projet est deja enregistre, renvoie false
	if (projectId(project) != -1) return(false);
	
	// enregistre le projet
	registered_projects_.insert(next_project_id ++, project);
	return(true);
}

/**
	Annule l'enregistrement du projet project
	@param project Projet dont il faut annuler l'enregistrement
	@return true si l'annulation a reussi, false sinon
	L'echec de cette methode signifie generalement que le projet n'etait pas enregistre.
*/
bool QETApp::unregisterProject(QETProject *project) {
	int project_id = projectId(project);
	
	// si le projet n'est pas enregistre, renvoie false
	if (project_id == -1) return(false);
	
	// annule l'enregistrement du projet
	return(registered_projects_.remove(project_id) == 1);
}

/**
	@param id Id du projet voulu
	@return le projet correspond a l'id passe en parametre
*/
QETProject *QETApp::project(const uint &id) {
	if (registered_projects_.contains(id)) {
		return(registered_projects_[id]);
	} else {
		return(0);
	}
}

/**
	@param project Projet dont on souhaite recuperer l'id
	@return l'id du projet en parametre si celui-ci est enregistre, -1 sinon
*/
int QETApp::projectId(const QETProject *project) {
	foreach(int id, registered_projects_.keys()) {
		if (registered_projects_[id] == project) {
			return(id);
		}
	}
	return(-1);
}
