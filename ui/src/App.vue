<script setup lang="ts">
import zhCn from 'element-plus/es/locale/lang/zh-cn'
import { ref, reactive, useTemplateRef } from 'vue'
import 'element-plus/es/components/message/style/css'
import 'element-plus/es/components/tree/style/css'
import { ElMessage, ElTree } from 'element-plus'
import type { CheckboxValueType } from 'element-plus'
import { CirclePlus, Remove } from '@element-plus/icons-vue'

interface RecipeTree {
  id: string
  label: string
  needCount: number
  craftedName: string
  craftedCount: number
  craftedMethodIndex: number
  completed: boolean
  children: RecipeTree[]
  parent: RecipeTree | null
}
// 定义 Ingredient 接口
interface Ingredient {
  id: string // 原料的 ID
  count: number // 原料的数量
}
// 定义 Recipe 接口
interface Recipe {
  id: string // 最终产物的 ID
  count: number // 最终产物的数量
  ingredients: Ingredient[] // 原料列表
}
// 定义 WrapperRecipe 接口
interface WrapperRecipe extends Recipe {
  rawIndex: number // 原始数据在数组中的索引
}
interface PrepareMaterials {
  key: string
  label: string
  craftedName: string
  count: number
  left: boolean
}
const props = {
  label: 'label',
  children: 'children',
}
const treeData = ref<RecipeTree[]>([])
const recipeTreeRef = useTemplateRef('recipeTreeRef')
const loading = ref(false)
const isAdding = ref(false)
const isAddingLock = ref(false)
const selectValue = ref<CheckboxValueType[]>([])
const optionName = ref('')
const recipe = ref<{ [key: string]: WrapperRecipe[] }>({})
const targetItem = ref<string[]>([])
const selectTargetItem = ref<{ value: string }[]>([])
const prepareMaterialsData = ref<PrepareMaterials[]>([])
const prepareMaterialsValue = ref<string[]>([])

const changeCraftedMethodIndex = (node: RecipeTree, index: number) => {
  const craftedTable = recipe.value[node.craftedName]
  if (craftedTable) {
    if (index >= 0 && index < craftedTable.length) {
      if (node.craftedMethodIndex != index) {
        const currentCrafted = craftedTable[index]
        node.craftedMethodIndex = index
        node.children = currentCrafted.ingredients.map((item) => {
          return {
            id: node.id + '_' + item.id,
            label: '',
            needCount: 0,
            craftedCount: 0,
            craftedName: item.id,
            craftedMethodIndex: -1,
            completed: node.completed,
            children: [],
            parent: node,
          } as RecipeTree
        })
        return
      }
    }
  }
  node.craftedMethodIndex = -1
  node.children = []
}
const computerCraftedResult = (recipeTree: RecipeTree) => {
  const craftedTable = recipe.value[recipeTree.craftedName]
  if (craftedTable) {
    if (recipeTree.craftedMethodIndex < 0 || recipeTree.craftedMethodIndex >= craftedTable.length) {
      recipeTree.craftedMethodIndex = -1
      recipeTree.craftedCount = 0
      recipeTree.label = recipeTree.craftedName + '（' + recipeTree.needCount + '）'
      recipeTree.children = []
      const parentCraftedName: { [key: string]: boolean } = {}
      let currentNode: RecipeTree | null = recipeTree
      while (currentNode) {
        parentCraftedName[currentNode.craftedName] = true
        currentNode = currentNode.parent
      }
      for (let i = 0; i < craftedTable.length; i++) {
        const current = craftedTable[i]
        let next = false
        for (let j = 0; j < current.ingredients.length; j++) {
          if (parentCraftedName[current.ingredients[j].id]) {
            next = true
            break
          }
        }
        if (!next) {
          recipeTree.craftedMethodIndex = i
          break
        }
      }
    }
    if (recipeTree.craftedMethodIndex >= 0 && recipeTree.craftedMethodIndex < craftedTable.length) {
      const currentCrafted = craftedTable[recipeTree.craftedMethodIndex]
      const craftedTimes = Math.ceil(recipeTree.needCount / currentCrafted.count)
      recipeTree.craftedCount = craftedTimes * currentCrafted.count
      if (recipeTree.children.length == currentCrafted.ingredients.length) {
        recipeTree.children.forEach((child, index) => {
          const item = currentCrafted.ingredients[index]
          child.id = recipeTree.id + '_' + item.id
          child.label = item.id + '（' + craftedTimes * item.count + '）'
          child.craftedName = item.id
          child.needCount = craftedTimes * item.count
          computerCraftedResult(child)
        })
      } else {
        const children: RecipeTree[] = currentCrafted.ingredients.map((item) => {
          const child: RecipeTree = {
            id: recipeTree.id + '_' + item.id,
            label: item.id + '（' + craftedTimes * item.count + '）',
            needCount: craftedTimes * item.count,
            craftedCount: 0,
            craftedName: item.id,
            craftedMethodIndex: -1,
            completed: recipeTree.completed,
            children: [],
            parent: recipeTree,
          }
          computerCraftedResult(child)
          return child
        })
        recipeTree.children = children
      }
      if (recipeTree.craftedCount == recipeTree.needCount) {
        recipeTree.label = recipeTree.craftedName + '（' + recipeTree.needCount + '）'
      } else {
        recipeTree.label = recipeTree.craftedName + '（' + recipeTree.needCount + '）'
      }
    }
  } else {
    recipeTree.craftedCount = 0
    recipeTree.craftedMethodIndex = -1
    recipeTree.label = recipeTree.craftedName + '（' + recipeTree.needCount + '）'
    recipeTree.children = []
  }
}
const computerCraftedDemand = (
  recipeTree: RecipeTree,
  prepareRedundancy: { [key: string]: number } = {},
  prepareDemand: { [key: string]: number } = {},
  completedDemand: { [key: string]: number } = {},
) => {
  if (recipeTree.completed) {
    if (recipeTree.parent == null || !recipeTree.parent.completed) {
      completedDemand[recipeTree.craftedName] =
        (completedDemand[recipeTree.craftedName] ?? 0) + recipeTree.needCount
    }
    return
  } else {
    const allCompleted = !recipeTree.children.some((child) => {
      return !child.completed
    })
    if (recipeTree.children.length == 0 || allCompleted) {
      prepareDemand[recipeTree.craftedName] =
        (prepareDemand[recipeTree.craftedName] ?? 0) + recipeTree.needCount
    }
  }
  prepareRedundancy[recipeTree.craftedName] = prepareRedundancy[recipeTree.craftedName] ?? 0
  if (recipeTree.craftedMethodIndex >= 0) {
    const craftedTable = recipe.value[recipeTree.craftedName]
    if (craftedTable) {
      const currentCrafted = craftedTable[recipeTree.craftedMethodIndex]
      recipeTree.craftedCount = 0
      recipeTree.children.forEach((child, index) => {
        if (prepareRedundancy[recipeTree.craftedName] > 0) {
          const minValue = Math.min(prepareRedundancy[recipeTree.craftedName], recipeTree.needCount)
          recipeTree.needCount -= minValue
          prepareRedundancy[recipeTree.craftedName] -= minValue
        }
        if (recipeTree.needCount > 0) {
          const craftedTimes = Math.ceil(recipeTree.needCount / currentCrafted.count)
          const item = currentCrafted.ingredients[index]
          child.needCount = craftedTimes * item.count
          recipeTree.craftedCount = craftedTimes * currentCrafted.count
          computerCraftedDemand(child, prepareRedundancy, prepareDemand, completedDemand)
        }
      })
      prepareRedundancy[recipeTree.craftedName] += recipeTree.craftedCount - recipeTree.needCount
    }
  }
}
const targetNumber = ref(1)
const refreshTreeData = () => {
  const prepareRedundancy: { [key: string]: number } = {}
  const prepareDemand: { [key: string]: number } = {}
  const completedDemand: { [key: string]: number } = {}
  treeData.value.forEach((item) => {
    computerCraftedResult(item)
    computerCraftedDemand(item, prepareRedundancy, prepareDemand, completedDemand)
  })
  prepareMaterialsData.value.forEach((item) => {
    item.count = 0
  })
  Object.entries(prepareDemand).forEach(([craftedName, totalSum]) => {
    const item = prepareMaterialsData.value
      .filter((item) => item.left)
      .find((item) => item.craftedName == craftedName)
    if (item) {
      item.count = totalSum
      item.label = craftedName + '（' + totalSum + '）'
    } else {
      prepareMaterialsData.value.push({
        key: 'L_' + craftedName,
        label: craftedName + '（' + totalSum + '）',
        craftedName: craftedName,
        count: totalSum,
        left: true,
      })
    }
  })
  Object.entries(completedDemand).forEach(([craftedName, totalSum]) => {
    const item = prepareMaterialsData.value
      .filter((item) => !item.left)
      .find((item) => item.craftedName == craftedName)
    if (item) {
      item.count = totalSum
      item.label = craftedName + '（' + totalSum + '）'
    } else {
      prepareMaterialsData.value.push({
        key: 'R_' + craftedName,
        label: craftedName + '（' + totalSum + '）',
        craftedName: craftedName,
        count: totalSum,
        left: false,
      })
      prepareMaterialsValue.value.push('R_' + craftedName)
    }
  })
  prepareMaterialsData.value = prepareMaterialsData.value.filter((item) => item.count != 0)
  prepareMaterialsValue.value = prepareMaterialsData.value
    .filter((item) => !item.left)
    .map((item) => item.key)
}
const onTargetNumberChange = () => {
  if (treeData.value.length > 0) {
    treeData.value[0].needCount = targetNumber.value
  }
  refreshTreeData()
}
const onAddOption = () => {
  isAdding.value = true
}
const onSelected = (v: string) => {
  const recipeTree: RecipeTree = {
    id: v,
    label: v,
    needCount: targetNumber.value,
    craftedCount: 0,
    craftedName: v,
    craftedMethodIndex: -1,
    completed: false,
    children: [],
    parent: null,
  }
  treeData.value.pop()
  treeData.value.push(recipeTree)
  refreshTreeData()
}
const includeIngredient = ref(false)
const showSelectTargetItem = () => {
  if (includeIngredient.value) {
    selectTargetItem.value = Array.from(
      new Set(
        targetItem.value
          .map((item) => item) // 先提取出原始的 value 值
          .concat(
            Object.keys(recipe.value).map((item) => item), // 提取 recipe 的 key 值
          ),
      ),
    ).map((value) => {
      return { value: value } // 重新转换为对象格式
    })
  } else {
    selectTargetItem.value = targetItem.value.map((item) => {
      return { value: item }
    })
  }
}
const onConfirm = () => {
  if (optionName.value) {
    if (!isAddingLock.value) {
      isAddingLock.value = true
      window.safeWebui
        .call('addTargetItem', optionName.value)
        .then((response) => {
          targetItem.value = JSON.parse(response) as string[]
          showSelectTargetItem()
          clear()
        })
        .catch((error) => {
          ElMessage.error(error.message)
        })
        .finally(() => {
          isAddingLock.value = false
        })
    }
  }
}
const clear = () => {
  optionName.value = ''
  isAdding.value = false
}
const parseRecipeData = (data: string) => {
  const recipeData = JSON.parse(data) as Recipe[]
  const recipeMap: { [key: string]: WrapperRecipe[] } = {}
  recipeData.forEach((item, index) => {
    if (recipeMap[item.id]) {
      recipeMap[item.id].push({
        id: item.id,
        count: item.count,
        ingredients: item.ingredients,
        rawIndex: index,
      })
    } else {
      recipeMap[item.id] = [
        { id: item.id, count: item.count, ingredients: item.ingredients, rawIndex: index },
      ]
    }
  })
  recipe.value = recipeMap
}
const initRecipeData = () => {
  window.safeWebui
    .call('getRecipeData')
    .then((response) => {
      parseRecipeData(response)
    })
    .catch(() => {
      setTimeout(initRecipeData, 100)
    })
}
const initTargetItemData = () => {
  window.safeWebui
    .call('getTargetItemData')
    .then((response) => {
      targetItem.value = JSON.parse(response) as string[]
      showSelectTargetItem()
    })
    .catch(() => {
      setTimeout(initTargetItemData, 100)
    })
}
initRecipeData()
initTargetItemData()
// do not use same name with ref
const form = reactive({
  recipe: {
    id: '',
    label: '',
    needCount: 0,
    craftedCount: 0,
    craftedName: '',
    craftedMethodIndex: -1,
    completed: false,
    children: [],
    parent: null,
  } as RecipeTree,
  count: 1,
  recipeMethod: [] as { value: number; label: string }[],
  ingredient: [] as Ingredient[],
  recipeSelect: -1,
})
const refreshRecipeSelect = (newSelectIndex: number) => {
  const tempRecipeMethod: { value: number; label: string }[] = []
  const recipeData = recipe.value[form.recipe.craftedName]
  if (recipeData) {
    for (let index = 0; index < recipeData.length; index++) {
      tempRecipeMethod.push({ value: index, label: '第' + (index + 1) + '种合成' })
    }
    form.count = recipeData[newSelectIndex].count
  } else {
    form.count = 1
  }
  form.recipeMethod = tempRecipeMethod
  if (tempRecipeMethod.length > newSelectIndex) {
    form.recipeSelect = newSelectIndex
    form.ingredient = recipeData[newSelectIndex].ingredients.map((item) => {
      return { id: item.id, count: item.count }
    })
  } else {
    form.recipeSelect = -1
    form.ingredient = []
  }
}
const onSubmit = () => {
  if (form.recipeSelect === -1) {
    const newSelectIndex = form.recipeMethod.length
    window.safeWebui
      .call(
        'addRecipe',
        JSON.stringify({
          id: form.recipe.craftedName,
          count: form.count,
          ingredients: form.ingredient,
        }),
      )
      .then((response) => {
        parseRecipeData(response)
        refreshRecipeSelect(newSelectIndex)
        changeCraftedMethodIndex(form.recipe, newSelectIndex)
        refreshTreeData()
        ElMessage({
          message: '添加成功！',
          type: 'success',
        })
      })
      .catch(() => {
        ElMessage.error('添加失败！')
      })
  } else {
    const oldSelectIndex = form.recipeSelect
    window.safeWebui
      .call(
        'updateRecipe',
        recipe.value[form.recipe.craftedName][oldSelectIndex].rawIndex,
        JSON.stringify({
          id: form.recipe.craftedName,
          count: form.count,
          ingredients: form.ingredient,
        }),
      )
      .then((response) => {
        parseRecipeData(response)
        refreshRecipeSelect(oldSelectIndex)
        changeCraftedMethodIndex(form.recipe, oldSelectIndex)
        refreshTreeData()
        ElMessage({
          message: '修改成功！',
          type: 'success',
        })
      })
      .catch(() => {
        ElMessage.error('修改失败！')
      })
  }
}
const onTreeCurrentChange = (data: RecipeTree) => {
  form.recipe = data
  refreshRecipeSelect(0)
}

const onRecipeChange = (value: number) => {
  if (value === -1) {
    form.ingredient = []
    form.count = 1
  } else {
    form.ingredient = recipe.value[form.recipe.craftedName][value].ingredients.map((item) => {
      return { id: item.id, count: item.count }
    })
    form.count = recipe.value[form.recipe.craftedName][value].count
  }
}
const onCancel = () => {
  form.recipeSelect = form.recipe.craftedMethodIndex
  onRecipeChange(form.recipeSelect)
}
const onTreeCheckChange = (data: RecipeTree, checked: boolean) => {
  if (checked) {
    const forEachChildrenChecked = (node: RecipeTree) => {
      node.children.forEach((child) => {
        forEachChildrenChecked(child)
        recipeTreeRef.value?.setChecked(child, true, false)
        child.completed = true
      })
    }
    forEachChildrenChecked(data)
  } else {
    let parentNode = data.parent
    while (parentNode != null) {
      parentNode.completed = false
      recipeTreeRef.value?.setChecked(parentNode, false, false)
      parentNode = parentNode.parent
    }
  }
  data.completed = checked
  refreshTreeData()
}

const filterMethod = (query: string, item: PrepareMaterials) => {
  return item.label.toLowerCase().includes(query.toLowerCase())
}
const prepareChange = (value: string[], direction: 'left' | 'right', movedKeys: string[]) => {
  const movedCraftedName = prepareMaterialsData.value
    .filter((item) => movedKeys.includes(item.key))
    .map((item) => item.craftedName)
  if (direction === 'left') {
    treeData.value.forEach((data) => {
      const forEachChildrenChecked = (node: RecipeTree) => {
        if (node.parent == null || !node.parent.completed) {
          if (movedCraftedName.includes(node.craftedName)) {
            recipeTreeRef.value?.setChecked(node, false, false)
            node.completed = false
          }
          node.children.forEach((child) => {
            forEachChildrenChecked(child)
          })
        }
      }
      forEachChildrenChecked(data)
    })
  } else if (direction === 'right') {
    treeData.value.forEach((data) => {
      const forEachChildrenChecked = (node: RecipeTree) => {
        if (movedCraftedName.includes(node.craftedName)) {
          const allCompleted = !node.children.some((child) => {
            return !child.completed
          })
          if (node.children.length == 0 || allCompleted) {
            recipeTreeRef.value?.setChecked(node, true, false)
            node.completed = true
          }
        }
        if (!node.completed) {
          node.children.forEach((child) => {
            forEachChildrenChecked(child)
          })
        }
      }
      forEachChildrenChecked(data)
    })
  }
  refreshTreeData()
}
</script>

<template>
  <el-config-provider :locale="zhCn">
    <div class="fl">
      <el-text class="mx-1">选择要最终合成</el-text>
      <div class="s1"></div>
      <el-input-number @change="onTargetNumberChange" v-model="targetNumber" :min="1" />
      <div class="s1"></div>
      <div>个</div>
      <div class="s1"></div>
      <el-select
        v-model="selectValue"
        filterable
        placeholder="选择一个目标物品"
        @change="onSelected"
        style="width: 240px"
      >
        <el-option
          v-for="item in selectTargetItem"
          :key="item.value"
          :label="item.value"
          :value="item.value"
        />
        <template #footer>
          <el-button v-if="!isAdding" text bg size="small" @click="onAddOption">
            添加物品
          </el-button>
          <template v-else>
            <el-input
              v-model="optionName"
              class="option-input"
              placeholder="input option name"
              size="small"
            />
            <div class="s1"></div>
            <el-button type="primary" size="small" @click="onConfirm">确认</el-button>
            <el-button size="small" @click="clear">cancel</el-button>
          </template>
        </template>
      </el-select>
      <div class="s1"></div>
      <el-checkbox
        @change="showSelectTargetItem"
        v-model="includeIngredient"
        label="包含原料"
        size="large"
      />
    </div>
    <div class="s4"></div>
    <div class="f fg">
      <div class="fcl fg">
        <el-text class="mx-1">合成树</el-text>
        <div class="s1"></div>
        <div class="flc"></div>
        <div class="s1"></div>
        <el-tree
          class="fgs"
          ref="recipeTreeRef"
          v-loading="loading"
          :data="treeData"
          :props="props"
          check-strictly
          node-key="id"
          show-checkbox
          highlight-current
          @current-change="onTreeCurrentChange"
          @check-change="onTreeCheckChange"
        />
      </div>
      <div class="s2"></div>
      <div class="fcl">
        <el-text class="mx-1">添加合成</el-text>
        <div class="s1"></div>
        <div class="flc"></div>
        <div class="s1"></div>
        <el-form class="fgs" :model="form" label-width="auto" style="width: 520px">
          <el-form-item label="目标物品名称">
            <el-input v-model="form.recipe.craftedName" disabled />
          </el-form-item>
          <el-form-item label="合成路线">
            <el-select
              @change="onRecipeChange"
              v-model="form.recipeSelect"
              placeholder="选择合成路线"
            >
              <el-option
                v-for="item in form.recipeMethod"
                :key="item.value"
                :label="item.label"
                :value="item.value"
              />
              <el-option label="添加新合成" :value="-1" />
            </el-select>
          </el-form-item>
          <el-form-item label="目标物品数量">
            <el-input-number v-model="form.count" :min="1" />
          </el-form-item>
          <el-form-item label="所需材料">
            <div class="fcl">
              <div class="fcl" v-for="(item, index) in form.ingredient" :key="index">
                <div class="fl">
                  <el-input-number v-model="item.count" :min="1" placeholder="数量" />
                  <div class="s1"></div>
                  <el-text class="mx-1">个</el-text>
                  <div class="s1"></div>
                  <el-input v-model="item.id" placeholder="原料名称" />
                  <div class="s1"></div>
                  <el-icon @click="form.ingredient.splice(index, 1)" :size="20" color="#f56c6c">
                    <Remove />
                  </el-icon>
                </div>
                <div class="s1"></div>
              </div>
              <el-tooltip class="box-item" effect="dark" content="追加材料" placement="top">
                <el-icon
                  @click="form.ingredient.push({ id: '', count: 1 })"
                  :size="20"
                  color="#409efc"
                >
                  <CirclePlus />
                </el-icon>
              </el-tooltip>
            </div>
          </el-form-item>
          <el-form-item>
            <el-button type="primary" @click="onSubmit" :disabled="form.recipe.craftedName == ''"
              >确认</el-button
            >
            <el-button @click="onCancel" :disabled="form.recipe.craftedName == ''">取消</el-button>
          </el-form-item>
        </el-form>
      </div>
      <div class="s2"></div>
      <div class="fcl">
        <el-text class="mx-1">准备材料</el-text>
        <div class="s1"></div>
        <div class="flc"></div>
        <div class="s1"></div>
        <el-transfer
          class="prepare fl fg"
          v-model="prepareMaterialsValue"
          filterable
          :filter-method="filterMethod"
          filter-placeholder="输入材料名称搜索"
          :data="prepareMaterialsData"
          :titles="['需要准备的材料', '已准备的材料']"
          @change="prepareChange"
        >
          <template #default="{ option }">
            <span :title="option.label">{{ option.label }}</span>
          </template>
        </el-transfer>
      </div>
    </div>
  </el-config-provider>
</template>

<style scoped>
.prepare {
  height: 0;
}
.prepare > :deep(.el-transfer-panel) {
  display: flex;
  flex-direction: column;
  align-self: normal;
}
.prepare > :deep(.el-transfer-panel > .el-transfer-panel__body) {
  flex-grow: 1;
}
.prepare > :deep(.el-transfer__buttons) {
  display: flex;
  flex-direction: column;
}
.prepare > :deep(.el-transfer__buttons > .el-transfer__button:nth-child(1)) {
  order: 0;
}
.prepare > :deep(.el-transfer__buttons::after) {
  content: '';
  display: block;
  height: 3vh;
  order: 1;
}
.prepare > :deep(.el-transfer__buttons > .el-transfer__button:nth-child(2)) {
  margin: 0;
  order: 2;
}
</style>
