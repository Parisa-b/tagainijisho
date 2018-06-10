/*
 *  Copyright (C) 2010  Alexandre Courbot
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gui/jmdict/JMdictFilterWidget.h"
#include "core/jmdict/JMdictPlugin.h"
#include "core/jmdict/JMdictEntrySearcher.h"
#include "gui/KanjiValidator.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QRegularExpression>

struct MenuHierarchyEntry {
	QRegularExpression regex;
	QString title;
	QString parent;
};

// TODO pass this as an argument
static QList<MenuHierarchyEntry> posHierarchy = {
	{ QRegularExpression("(v1.*|vz)"), "Ichidan verb", "v.+" },
	{ QRegularExpression("v2.*"), "Nidan verb", "v.+" },
	{ QRegularExpression("v4.*"), "Yodan verb", "v.+" },
	{ QRegularExpression("v5.*"), "Godan verb", "v.+" },
	{ QRegularExpression("adj.*"), "Adjective", NULL },
	{ QRegularExpression("adv.*"), "Adverb", NULL },
	{ QRegularExpression("(n|n-.+)"), "Noun", NULL },
	{ QRegularExpression("(aux|aux-.+)"), "Auxiliary", NULL },
	{ QRegularExpression("v.+"), "Verb", NULL },
};

static QMenu *findParentMenu(const MenuHierarchyEntry &foo, QMap<QString, QMenu *> &subMenus, QMenu *root, const QList<MenuHierarchyEntry> &hierarchy)
{
	QMenu *parent = NULL;
	QMenu *ret;

	// Already exists? Just return it
	ret = subMenus[foo.regex.pattern()];
	if (ret)
		return ret;

	// Else find our parent
	if (foo.parent == NULL)
		parent = root;
	else {
		for (auto pair : hierarchy)
			if (pair.regex.pattern() == foo.parent) {
				parent = findParentMenu(pair, subMenus, root, hierarchy);
				break;
			}
	}

	// Fallback in case we messed up our hierarchy
	if (parent == NULL)
		parent = root;

	// And create a menu under it
	ret = new QMenu(parent);
	// TODO translate!
	ret->setTitle(foo.title);
	parent->addAction(ret->menuAction());
	subMenus[foo.regex.pattern()] = ret;

	return ret;
}

static QActionGroup *addCheckableProperties(const QVector<QPair<QString, QString> >&defs, QMenu *menu, const QList<MenuHierarchyEntry> &hierarchy = {})
{
	QList<QString> strList;
	QMap<QString, QMenu *> subMenus;

	for (int i = 0; i < defs.size(); i++) {
		QString translated = QCoreApplication::translate("JMdictLongDescs", defs[i].second.toLatin1());
		strList << QString(translated.replace(0, 1, translated[0].toUpper()));
	}
	//QStringList sortedList(strList);
	//qSort(sortedList.begin(), sortedList.end());
	QActionGroup *actionGroup = new QActionGroup(menu);
	actionGroup->setExclusive(false);
	for (auto str : strList) {
		int idx = strList.indexOf(str);
		QMenu *menuToAdd = menu;
		QAction *action = actionGroup->addAction(str);
		action->setCheckable(true);
		action->setProperty("TJpropertyIndex", idx);

		QString abbr(defs[idx].first);
		for (auto &pair : hierarchy)
			if (pair.regex.match(abbr, 0, QRegularExpression::NormalMatch, QRegularExpression::AnchoredMatchOption).hasMatch()) {
				menuToAdd = findParentMenu(pair, subMenus, menu, hierarchy);
				break;
			}

		menuToAdd->addAction(action);
	}
	return actionGroup;
}

JMdictFilterWidget::JMdictFilterWidget(QWidget *parent) : SearchFilterWidget(parent, "wordsdic")
{
	_propsToSave << "containedKanjis" << "containedComponents" << "studiedKanjisOnly" << "kanaOnlyWords" << "pos" << "dial" << "field" << "misc";
	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	{
		_containedKanjis = new QLineEdit(this);
		KanjiValidator *kanjiValidator = new KanjiValidator(this);
		_containedKanjis->setValidator(kanjiValidator);
		_containedComponents = new QLineEdit(this);
		_containedComponents->setValidator(kanjiValidator);

		QGridLayout *gLayout = new QGridLayout();
		gLayout->addWidget(new QLabel(tr("With kanji:"), this), 0, 0);
		gLayout->addWidget(_containedKanjis, 0, 1);
		_studiedKanjisCheckBox = new QCheckBox(tr("Using studied kanji only"));
		gLayout->addWidget(_studiedKanjisCheckBox, 0, 2);

		gLayout->addWidget(new QLabel(tr("With components:"), this), 1, 0);
		gLayout->addWidget(_containedComponents, 1, 1);
		_kanaOnlyCheckBox = new QCheckBox(tr("Include kana-only words"));
		gLayout->addWidget(_kanaOnlyCheckBox, 1, 2);

		mainLayout->addLayout(gLayout);
		connect(_containedKanjis, SIGNAL(textChanged(const QString &)), this, SLOT(commandUpdate()));
		connect(_containedComponents, SIGNAL(textChanged(const QString &)), this, SLOT(commandUpdate()));
		connect(_studiedKanjisCheckBox, SIGNAL(toggled(bool)), this, SLOT(commandUpdate()));
		connect(_kanaOnlyCheckBox, SIGNAL(toggled(bool)), this, SLOT(commandUpdate()));

	}
	{
		QHBoxLayout *hLayout = new QHBoxLayout();

		_posButton = new QPushButton(tr("Part of speech"), this);
		QMenu *menu = new QMenu(this);
		QActionGroup *actionGroup = addCheckableProperties(JMdictPlugin::posEntities(), menu, posHierarchy);
		_posButton->setMenu(menu);
		hLayout->addWidget(_posButton);
		connect(actionGroup, SIGNAL(triggered(QAction *)), this, SLOT(onPosTriggered(QAction *)));
		_dialButton = new QPushButton(tr("Dialect"), this);
		menu = new QMenu(this);
		actionGroup = addCheckableProperties(JMdictPlugin::dialectEntities(), menu);
		_dialButton->setMenu(menu);
		hLayout->addWidget(_dialButton);
		connect(actionGroup, SIGNAL(triggered(QAction *)), this, SLOT(onDialTriggered(QAction *)));
		_fieldButton = new QPushButton(tr("Field"), this);
		menu = new QMenu(this);
		actionGroup = addCheckableProperties(JMdictPlugin::fieldEntities(), menu);
		_fieldButton->setMenu(menu);
		hLayout->addWidget(_fieldButton);
		connect(actionGroup, SIGNAL(triggered(QAction *)), this, SLOT(onFieldTriggered(QAction *)));
		_miscButton = new QPushButton(tr("Misc"), this);
		menu = new QMenu(this);
		actionGroup = addCheckableProperties(JMdictPlugin::miscEntities(), menu);
		_miscButton->setMenu(menu);
		hLayout->addWidget(_miscButton);
		connect(actionGroup, SIGNAL(triggered(QAction *)), this, SLOT(onMiscTriggered(QAction *)));
		updateMiscFilteredProperties();
		connect(&JMdictEntrySearcher::miscPropertiesFilter, SIGNAL(valueChanged(QVariant)), this, SLOT(updateMiscFilteredProperties()));

		mainLayout->addLayout(hLayout);
	}
	mainLayout->setContentsMargins(0, 0, 0, 0);
}

QString JMdictFilterWidget::currentCommand() const
{
	QString ret;
	QString kanjis = _containedKanjis->text();
	if (!kanjis.isEmpty()) {
		bool first = true;
		ret += " :haskanji=";
		foreach(QChar c, kanjis) {
			if (!first) ret +=",";
			else first = false;
			ret += QString("\"%1\"").arg(c);
		}
	}
	if (_studiedKanjisCheckBox->isChecked()) ret += " :withstudiedkanjis";
	if (_kanaOnlyCheckBox->isChecked()) ret += " :withkanaonly";
	kanjis = _containedComponents->text();
	if (!kanjis.isEmpty()) {
		bool first = true;
		ret += " :hascomponent=";
		foreach(QChar c, kanjis) {
			if (!first) ret +=",";
			else first = false;
			ret += QString("\"%1\"").arg(c);
		}
	}
	if (!_posList.isEmpty()) ret += " :pos=" + _posList.join(",");
	if (!_dialList.isEmpty()) ret += " :dial=" + _dialList.join(",");
	if (!_fieldList.isEmpty()) ret += " :field=" + _fieldList.join(",");
	if (!_miscList.isEmpty()) ret += " :misc=" + _miscList.join(",");
	return ret;
}

QString JMdictFilterWidget::currentTitle() const
{
	QString contains;
	QString kanjis = _containedKanjis->text();
	QString comps = _containedComponents->text();
	if (!kanjis.isEmpty()) {
		bool first = true;
		contains += tr(" with ");
		foreach(QChar c, kanjis) {
			if (!first) contains +=",";
			else first = false;
			contains += c;
		}
	}
	if (!comps.isEmpty()) {
		bool first = true;
		contains += tr(" with component ");
		foreach(QChar c, comps) {
			if (!first) contains +=",";
			else first = false;
			contains += c;
		}
	}
	if (_studiedKanjisCheckBox->isChecked()) {
		if (!kanjis.isEmpty() || !comps.isEmpty()) contains += tr(", studied kanji only");
		else contains += tr(" with studied kanji");
	}
	if (_kanaOnlyCheckBox->isChecked()) {
		if (!kanjis.isEmpty() || !comps.isEmpty() || _studiedKanjisCheckBox->isChecked()) contains += tr(", including kana words");
		else contains += tr(" using kana only");
	}

	QStringList propsList = _posList + _dialList + _fieldList + _miscList;
	QString props = propsList.join(",");

	if (!props.isEmpty()) props = " (" + props + ")";
	QString ret(props + contains);
	if (!ret.isEmpty()) ret = tr("Vocabulary") + ret;
	else ret = tr("Vocabulary");
	return ret;
}

void JMdictFilterWidget::onPosTriggered(QAction *action)
{
	if (action->isChecked()) {
		int propertyIndex = action->property("TJpropertyIndex").toInt();
		_posList << JMdictPlugin::posEntities()[propertyIndex].first;
	}
	else {
		int propertyIndex = action->property("TJpropertyIndex").toInt();
		_posList.removeOne(JMdictPlugin::posEntities()[propertyIndex].first);
	}
	if (!_posList.isEmpty()) _posButton->setText(tr("Pos:") + _posList.join(","));
	else _posButton->setText(tr("Part of speech"));
	commandUpdate();
}

void JMdictFilterWidget::onDialTriggered(QAction *action)
{
	if (action->isChecked()) {
		int propertyIndex = action->property("TJpropertyIndex").toInt();
		_dialList << JMdictPlugin::dialectEntities()[propertyIndex].first;
	}
	else {
		int propertyIndex = action->property("TJpropertyIndex").toInt();
		_dialList.removeOne(JMdictPlugin::dialectEntities()[propertyIndex].first);
	}
	if (!_dialList.isEmpty()) _dialButton->setText(tr("Dial:") + _dialList.join(","));
	else _dialButton->setText(tr("Dialect"));
	commandUpdate();
}

void JMdictFilterWidget::onFieldTriggered(QAction *action)
{
	if (action->isChecked()) {
		int propertyIndex = action->property("TJpropertyIndex").toInt();
		_fieldList << JMdictPlugin::fieldEntities()[propertyIndex].first;
	}
	else {
		int propertyIndex = action->property("TJpropertyIndex").toInt();
		_fieldList.removeOne(JMdictPlugin::fieldEntities()[propertyIndex].first);
	}
	if (!_fieldList.isEmpty()) _fieldButton->setText(tr("Field:") + _fieldList.join(","));
	else _fieldButton->setText(tr("Field"));
	emit commandUpdate();
}

void JMdictFilterWidget::onMiscTriggered(QAction *action)
{
	if (action->isChecked()) {
		int propertyIndex = action->property("TJpropertyIndex").toInt();
		_miscList << JMdictPlugin::miscEntities()[propertyIndex].first;
	}
	else {
		int propertyIndex = action->property("TJpropertyIndex").toInt();
		_miscList.removeOne(JMdictPlugin::miscEntities()[propertyIndex].first);
	}
	if (!_miscList.isEmpty()) _miscButton->setText(tr("Misc:") + _miscList.join(","));
	else _miscButton->setText(tr("Misc"));
	emit commandUpdate();
}

void JMdictFilterWidget::_reset()
{
	_containedKanjis->clear();
	_containedComponents->clear();
	_studiedKanjisCheckBox->setChecked(false);
	_kanaOnlyCheckBox->setChecked(false);
	foreach (QAction *action, _posButton->menu()->actions()) if (action->isChecked()) action->trigger();
	_posList.clear();
	foreach (QAction *action, _dialButton->menu()->actions()) if (action->isChecked()) action->trigger();
	_dialList.clear();
	foreach (QAction *action, _fieldButton->menu()->actions()) if (action->isChecked()) action->trigger();
	_fieldList.clear();
	foreach (QAction *action, _miscButton->menu()->actions()) if (action->isChecked()) action->trigger();
	_miscList.clear();
}

void JMdictFilterWidget::updateFeatures()
{
	if (!currentCommand().isEmpty()) emit disableFeature("kanjidic");
	else emit enableFeature("kanjidic");
}

void JMdictFilterWidget::setPos(const QStringList &list)
{
	_posList.clear();
	foreach(QAction *action, _posButton->menu()->actions()) {
		if (action->isChecked()) action->trigger();
		if (list.contains(JMdictPlugin::posEntities()[action->property("TJpropertyIndex").toInt()].first))
			action->trigger();
	}
}

void JMdictFilterWidget::setDial(const QStringList &list)
{
	_dialList.clear();
	foreach(QAction *action, _dialButton->menu()->actions()) {
		if (action->isChecked()) action->trigger();
		if (list.contains(JMdictPlugin::dialectEntities()[action->property("TJpropertyIndex").toInt()].first))
			action->trigger();
	}
}

void JMdictFilterWidget::setField(const QStringList &list)
{
	_fieldList.clear();
	foreach(QAction *action, _fieldButton->menu()->actions()) {
		if (action->isChecked()) action->trigger();
		if (list.contains(JMdictPlugin::fieldEntities()[action->property("TJpropertyIndex").toInt()].first))
			action->trigger();
	}
}

void JMdictFilterWidget::setMisc(const QStringList &list)
{
	_miscList.clear();
	foreach(QAction *action, _miscButton->menu()->actions()) {
		if (action->isChecked()) action->trigger();
		if (list.contains(JMdictPlugin::miscEntities()[action->property("TJpropertyIndex").toInt()].first))
			action->trigger();
	}
}
